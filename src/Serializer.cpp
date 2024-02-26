#include <n2ajl/Serializer.h>

namespace n2ajl
{

void SerializeNode(const Node& n, std::string& out, size_t depth, const SerializerConfig& cfg);

void SerializeBoolean(const Node& n, std::string& out)
{
	out += n.GetBool() ? "true" : "false";
}

void SerializeNumber(const Node& n, std::string& out)
{
	std::string str = std::to_string(n.GetNumber());

	if (str.find('.') != std::string::npos)
	{
		// remove any trailing zeros
		while (!str.empty() && *str.rbegin() == '0')
			str.pop_back();

		// if the last position is a decimal point, strip it
		if (str.size() > 1 && *str.rbegin() == '.')
			str.pop_back();
	}

	out += str;
}

void Indent(const SerializerConfig& cfg, size_t depth, std::string& out)
{
	switch (cfg.m_eIndentation)
	{
		case SerializerConfig::Indentation::FourSpace:
			for (size_t i = 0; i < depth * 4; i++)
				out += ' ';
			break;
		case SerializerConfig::Indentation::TwoSpace:
			for (size_t i = 0; i < depth * 2; i++)
				out += ' ';
			break;
		case SerializerConfig::Indentation::Tab:
			for (size_t i = 0; i < depth; i++)
				out += '\t';
			break;
		default: break;
	}
}

void SerializeObject(const Node& n, std::string& out, size_t depth, const SerializerConfig& cfg)
{
	bool bFirst = true;
	size_t uCount = 0;

	out += '{';

	n.ForEachMember([&](const std::string& szLabel, const Node& member)
					{
						if (cfg.m_bFancy)
							out += " \n";

						if (cfg.m_bFancy)
							Indent(cfg, depth + 1, out);

						out += '\"' + szLabel + "\":";

						if (cfg.m_bFancy)
							out += ' ';

						SerializeNode(member, out, depth + 1, cfg);
						uCount++;

						if (uCount != n.GetNumMembers())
							out += ',';

						bFirst = false;
					});

	if (cfg.m_bFancy && uCount)
	{
		out += '\n';
		Indent(cfg, depth, out);
	}

	out += '}';
}

void SerializeArray(const Node& n, std::string& out, size_t depth, const SerializerConfig& cfg)
{
	bool bFirst = true;
	size_t uCount = 0;

	out += '[';

	n.ForEachElement([&](const Node& element)
					{
						if (!bFirst && cfg.m_bFancy)
							out += ' ';

						if (cfg.m_bFancy)
							out += '\n';

						if (cfg.m_bFancy)
							Indent(cfg, depth + 1, out);

						SerializeNode(element, out, depth + 1, cfg);
						uCount++;

						if (uCount != n.Length())
							out += ',';

						bFirst = false;
					});

	if (cfg.m_bFancy && uCount)
	{
		out += '\n';
		Indent(cfg, depth, out);
	}

	out += ']';
}

void SerializeNode(const Node& n, std::string& out, size_t depth, const SerializerConfig& cfg)
{
	switch (n.GetType())
	{
		case Node::Type::Null:
			out += "null";
			break;
		case Node::Type::Boolean:
			SerializeBoolean(n, out);
			break;
		case Node::Type::Number:
			SerializeNumber(n, out);
			break;
		case Node::Type::String:
			out += '\"' + n.GetString() + '\"';
			break;
		case Node::Type::Array:
			SerializeArray(n, out, depth, cfg);
			break;
		case Node::Type::Object:
			SerializeObject(n, out, depth, cfg);
			break;
	}
}

utf8string Serialize(const SerializerConfig& cfg, const Node& json)
{
	std::string out;
	SerializeNode(json, out, 0, cfg);

	return out;
}

}