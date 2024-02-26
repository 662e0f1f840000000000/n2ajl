#include <n2ajl/Parser.h>
#include <n2ajl/UTF.h>

namespace n2ajl
{

thread_local char szError[256] = {};

inline bool IsWhitespace(utf32_t ch)
{
	return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r';
}

inline bool IsLiteralTerminator(uint32_t ch)
{
	return ch == ',' || ch == ']' || ch == '}';
}

bool SkipWhitespace(UTF8Iterator& iter)
{
	uint32_t h = iter.Read();
	if (!h)
	{
		snprintf(szError, sizeof(szError), "Unexpected end of stream");
		return false;
	}

	// trim leading whitespace
	while (h && IsWhitespace(h))
	{
		iter.Advance();
		h = iter.Read();
	}

	return true;
}

// TODO: handle Unicode escape sequences (\uXXXX)
bool GetNextString(UTF8Iterator& iter, std::string& str)
{
	str = "";
	if (!SkipWhitespace(iter))
		return false;

	uint32_t ch = iter.Read();
	bool bEscape = false;

	if (ch != '\"')
		return false; // doesn't start with quotes...

	iter.Advance(); // skip first quotes

	// we record the start ptr after advancing and advance after copying to omit the quotes
	auto* start = iter.GetReadPtr();

	while (ch = iter.Read())
	{
		if (ch == '\\' && !bEscape) // if we hit an escape character, and we're not escaping...
		{
			bEscape = true; // treat next char as a literal
			iter.Advance();
			continue;
		}
		else if (ch == '\"') // and we hit a double quote
		{
			if (!bEscape) // if not escaped, terminate the string
			{
				size_t len = iter.GetReadPtr() - start;
				str.resize(len);

				memcpy(const_cast<char*>(str.data()), start, len);

				if (!iter.Advance())
					return false;

				return true;
			}
		}

		bEscape = false;
		iter.Advance();
	}

	return false; // non-terminated string...
}

bool GetNextLiteral(UTF8Iterator& iter, std::string& str)
{
	str = "";
	if (!SkipWhitespace(iter))
		return false;

	uint32_t ch = iter.Read();

	if (ch == '\"') // special case
		return GetNextString(iter, str);

	auto* start = iter.GetReadPtr();

	while (ch = iter.Read())
	{
		if (ch >= 0x7F)
			return false;

		if (IsLiteralTerminator(ch) || IsWhitespace(ch))
		{
			if (iter.GetReadPtr() != start)
			{
				size_t len = iter.GetReadPtr() - start;
				str.resize(len);

				memcpy(const_cast<char*>(str.data()), start, len);
				return true;
			}
			else
			{
				return false;
			}
		}

		iter.Advance();
	}

	return false; // non-terminated string...
}

// supports only 1 main scope which encapsulates an object or an array
Result GenerateNodes(UTF8Iterator& iter, size_t uCurDepth, size_t uMaxDepth, Node& n)
{
	n = Node(); // reset to null
	Node::Type eType = Node::Type::Null;

	bool bLabel = false;
	bool bTerminated = false;
	size_t uScopeStart = iter.GetPosition();
	size_t uLabelEnd = SIZE_MAX;
	size_t uMemberStart = SIZE_MAX;
	uint32_t ch = 0;
	std::string szCurLabel;

	// helper functions (dumps error into szError)

	auto AddNode = [&szCurLabel](Node& parent, const Node& inner)
	{
		switch (parent.GetType())
		{
			case Node::Type::Object:
				if (szCurLabel.empty())
				{
					snprintf(szError, sizeof(szError), "Internal error, missing label");
					return false;
				}

				parent.Set(szCurLabel, inner);
				szCurLabel.clear();
				break;
			case Node::Type::Array:
				parent.Append(inner);
				break;
			default:
				snprintf(szError, sizeof(szError), "Internal error, bad node type");
				return false;
		}

		return true;
	};

	auto StringAdvance = [&]()
	{
		if (!iter.Advance())
			return false;

		if (!SkipWhitespace(iter))
			return false;

		ch = iter.Read();
		return ch != '\0';
	};

	auto BuildSpanInner = [&]()
	{
		size_t uNextDepth = uCurDepth + 1;
		if (uNextDepth >= uMaxDepth)
		{
			snprintf(szError, sizeof(szError), "Too many nested spans at position %d", iter.GetPosition());
			return false;
		}

		Node inner;
		Result res = GenerateNodes(iter, uNextDepth, uMaxDepth, inner);

		if (!res.m_bSuccess) // failed
			return false;

		return AddNode(n, inner);
	};

	auto BuildSpanLiteral = [&]()
	{
		size_t uStartPos = iter.GetPosition();

		std::string str;
		if (!GetNextLiteral(iter, str))
		{
			snprintf(szError, sizeof(szError), "Malformed object, unexpected \'%c\' at position %d",
					 (char)ch, iter.GetPosition());
			return false;
		}

		switch (ch)
		{
			case '"':
			{
				return AddNode(n, Node(str));
			}
			case 't':
			case 'f':
			case 'n':
			{
				const utf8_t* z = str.c_str();
				uint64_t token = 0;

				switch (str.length())
				{
					case 5: // fallthrough
						token |= ((uint64_t)z[4] << 32);
					case 4:
						token |= (uint64_t)z[0] | ((uint64_t)z[1] << 8) | ((uint64_t)z[2] << 16) | ((uint64_t)z[3] << 24);
						break;
					default:
						snprintf(szError, sizeof(szError), "Malformed object, unexpected \'%c\' at position %d", (char)ch, uStartPos);
						return false;
				}

				if (token == 0x65757274) // true
				{
					return AddNode(n, Node(true));
				}
				else if (token == 0x65736c6166) // false
				{
					return AddNode(n, Node(false));
				}
				else if (token == 0x6c6c756e) // null
				{
					return AddNode(n, Node());
				}
				else
				{
					snprintf(szError, sizeof(szError), "Malformed object, unexpected \'%c\' at position %d", (char)ch, uStartPos);
					return false;
				}
			}
			default:
			{
				char* e;
				double num = strtod(str.c_str(), &e);

				if (e != str.c_str() + str.length()) // did not reach end of string
				{
					snprintf(szError, sizeof(szError), "Failed to parse literal at position %d", uStartPos);
					return false;
				}

				return AddNode(n, Node(num));
			}
		}
	};

	auto ParseMember = [&]()
	{
		// try to parse the member...
		if (ch == '{' || ch == '[') // object or array span
		{
			if (!BuildSpanInner())
				return false;
		}
		else // try to parse as a literal
		{
			if (!BuildSpanLiteral())
				return false;
		}

		if (!SkipWhitespace(iter))
			return false;

		// re-read current character
		ch = iter.Read();

		return true;
	};

	auto CheckMemberTerminationAndAdvance = [&]()
	{
		if (!IsLiteralTerminator(ch)) // we're expecting a terminator after a member
		{
			snprintf(szError, sizeof(szError), "Malformed object, unexpected \'%c\' at position %d",
					 (char)ch,
					 iter.GetPosition());
			return false;
		}

		// reset member state
		bLabel = false;
		uLabelEnd = SIZE_MAX;
		uMemberStart = SIZE_MAX;

		if (ch == ',') // only skip comma, brackets are needed for span termination
			StringAdvance();

		return true;
	};

	if (!SkipWhitespace(iter)) { goto BuildSpanFail; } // reached end...

	while (ch = iter.Read())
	{
		// we only expect ASCII characters while parsing structures
		// string literals are handled in BuildSpanLiteral and the pointer is advanced
		if (ch >= 0x7F)
		{
			snprintf(szError, sizeof(szError), "Unexpected character at position %d", iter.GetPosition());
			goto BuildSpanFail;
		}

		// check span scope
		if (eType == Node::Type::Null) // start of span, check for opening characters
		{
			switch (ch)
			{
				case '{':
				{
					n = Node::Object();
					eType = n.GetType();
					break;
				}
				case '[':
				{
					n = Node::Array();
					eType = n.GetType();
					break;
				}
				default:
				{
					snprintf(szError, sizeof(szError), "Malformed object, found \'%c\' at position %d, expected start character",
							 (char)ch,
							 iter.GetPosition());
					goto BuildSpanFail;
				}
			}

			goto AdvanceChar;
		}
		else if ((ch == '}' && eType == Node::Type::Object) ||
				 (ch == ']' && eType == Node::Type::Array)) // check for end of span
		{
			// skip the terminator
			iter.Advance();

			bTerminated = true;
			break; // found end character, span completed
		}

		// we are currently iterating the span, do some parsing
		if (eType == Node::Type::Object)
		{
			if (!bLabel) // looking for a label for the next member
			{
				if (ch == '\"') // if the next character is a string...
				{
					bLabel = true; // we found a label
					size_t uStartPos = iter.GetPosition();

					std::string str; // get the label string
					if (!GetNextLiteral(iter, str))
					{
						snprintf(szError, sizeof(szError), "Malformed object, unexpected \'%c\' at position %d",
								 (char)ch,
								 uStartPos);
						goto BuildSpanFail;
					}
					else // ...record the label span and skip forward
					{
						if (str.empty())
						{
							snprintf(szError, sizeof(szError), "Empty identifier at position %d", uStartPos);
							goto BuildSpanFail;
						}

						szCurLabel = str;
						uLabelEnd = iter.GetPosition();

						if (!SkipWhitespace(iter))
							goto BuildSpanFail;

						continue;
					}
				}
				else
				{
					snprintf(szError, sizeof(szError), "Malformed object, expected '\"\' at position %d", iter.GetPosition());
					goto BuildSpanFail;
				}
			}
			else // we have a label, now we're looking for a member
			{
				if (uMemberStart == SIZE_MAX) // we do not have a member, look for a delimiter
				{
					if (ch == ':') // a member delimiter, next character marks the beginning of the member
					{
						uMemberStart = iter.GetPosition();
						goto AdvanceChar;
					}
					else // did not find what we were looking for...
					{
						snprintf(szError, sizeof(szError), "Malformed object, expected \':\' at position %d", iter.GetPosition());
						goto BuildSpanFail;
					}
				}
				else // next character is the member (object, array, literal, etc)
				{
					if (!ParseMember())
						goto BuildSpanFail; // szError contains failure message

					// check to see if the member was properly terminated
					if (!CheckMemberTerminationAndAdvance())
						goto BuildSpanFail;

					// do NOT advance one character, terminators need to be read by code above to complete span
					continue;
				}
			}
		}
		else if (eType == Node::Type::Array)
		{
			if (!ParseMember())
				goto BuildSpanFail; // szError contains failure message

			// all the array values need to be of the same type
			// array should never be empty (ParseMember)
			if (n.At(0)->GetType() != n.At(n.Length() - 1)->GetType())
			{
				snprintf(szError, sizeof(szError), "Malformed array, incorrect type at position %d", iter.GetPosition());
				goto BuildSpanFail;
			}

			// check to see if the member was properly terminated
			if (!CheckMemberTerminationAndAdvance())
				goto BuildSpanFail;

			// do NOT advance one character, terminators need to be read by code above to complete span
			continue;
		}
		else
		{
			snprintf(szError, sizeof(szError), "Bad span type");
			goto BuildSpanFail;
		}

	AdvanceChar:
		StringAdvance();
	}

	// check for a dangling label without a matching value
	if (eType == Node::Type::Object && uLabelEnd != SIZE_MAX)
	{
		snprintf(szError, sizeof(szError), "Expected an object member at position %d", uLabelEnd);
		goto BuildSpanFail;
	}

	// if the span was never terminated, it is malformed
	if (eType != Node::Type::Null && !bTerminated)
	{
		char chStart = ' ', chEnd = ' ';

		if (eType == Node::Type::Object)
		{
			chStart = '{';
			chEnd = '}';
		}
		else if (eType == Node::Type::Array)
		{
			chStart = '[';
			chEnd = ']';
		}

		snprintf(szError, sizeof(szError), R"(Expected a terminating '%c' for '%c' at position %d)",
				 chStart,
				 chEnd,
				 uScopeStart);

		goto BuildSpanFail;
	}

	return { true, "" };

BuildSpanFail:
	n = Node();
	return { false, szError };
}

Result Parse(const ParserConfig& cfg, const utf8_t* szJson, Node& json)
{
	UTF8Iterator iter(szJson);
	Result status = GenerateNodes(iter, 0, cfg.m_uMaxDepth, json);

	if (!status.m_bSuccess)
		return status;

	return { true, "" };
}

}
