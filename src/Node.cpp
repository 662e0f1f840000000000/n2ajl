#include <n2ajl/Node.h>

#define ON_TYPE_CHECK_FAIL { __debugbreak(); std::abort(); }
#define ENSURE_OBJECT { if (m_eType != Type::Object) ON_TYPE_CHECK_FAIL }
#define ENSURE_ARRAY { if (m_eType != Type::Array) ON_TYPE_CHECK_FAIL }

namespace n2ajl
{

Node::Node()
{
	memset(this, 0, sizeof(Node));
}

Node::~Node()
{
	Reset();
}

Node::Node(bool bValue)
{
	m_eType = Type::Boolean;
	m_bValue = bValue;
}

Node::Node(double dblValue)
{
	m_eType = Type::Number;
	m_dblValue = dblValue;
}

Node::Node(const utf8_t* szValue)
{
	m_eType = Type::String;
	new(&m_szValue) decltype(m_szValue);

	// decode the real length of the given Unicode string
	size_t uByteLength = 0;
	UTF8Iterator iter(szValue);

	while (iter.Advance())
	{
		iter.Read();
		uByteLength += iter.GetCodepointBytes();
		iter.Advance();
	}

	m_szValue.resize(uByteLength + 1);
	memcpy(const_cast<char*>(m_szValue.data()), szValue, uByteLength); // bad hack
	m_szValue[uByteLength] = '\0';
}

Node::Node(const utf8string& szValue)
{
	m_eType = Type::String;
	new(&m_szValue) decltype(m_szValue);
	m_szValue = szValue;
}

Node Node::Object()
{
	Node n;
	n.m_eType = Node::Type::Object;
	new(&n.m_mChildren) decltype(m_mChildren)();

	return n;
}

Node Node::Array()
{
	Node n;
	n.m_eType = Node::Type::Array;
	new(&n.m_vElements) decltype(m_vElements)();

	return n;
}

Node Node::String()
{
	Node n;
	n.m_eType = Node::Type::String;
	new(&n.m_szValue) decltype(m_szValue)();

	return n;
}

bool Node::GetBool() const
{
	if (m_eType != Type::Boolean)
	{
		__debugbreak();
		std::abort();
	}

	return m_bValue;
}

double Node::GetNumber() const
{
	if (m_eType != Type::Number)
	{
		__debugbreak();
		std::abort();
	}

	return m_dblValue;
}

const utf8string& Node::GetString() const
{
	if (m_eType != Type::String)
	{
		__debugbreak();
		std::abort();
	}

	return m_szValue;
}

Node& Node::operator=(const Node& RHS)
{
	// destroy our own non-trivially copyable members
	Reset();

	// copy node type
	m_eType = RHS.m_eType;

	// initialize union members
	switch (m_eType)
	{
		case Type::Boolean:
			m_bValue = RHS.m_bValue;
			break;
		case Type::Number:
			m_dblValue = RHS.m_dblValue;
			break;
		case Type::String:
			new(&m_szValue) decltype(m_szValue);
			m_szValue = RHS.m_szValue;
			break;
		case Type::Array:
			new(&m_vElements) decltype(m_vElements);
			m_vElements = RHS.m_vElements;
			break;
		case Type::Object:
			new(&m_mChildren) decltype(m_mChildren);
			m_mChildren = RHS.m_mChildren;
			break;
		default:
			break;
	}

	return *this;
}

Node::Node(const Node& RHS) noexcept
{
	memset(this, 0, sizeof(Node));
	operator=(RHS);
}

Node::Node(Node&& RHS) noexcept
{
	// destroy our own non-trivially copyable members
	Reset();

	// copy node type
	m_eType = RHS.m_eType;

	// initialize union members
	switch (m_eType)
	{
		case Type::Boolean:
			m_bValue = RHS.m_bValue;
			break;
		case Type::Number:
			m_dblValue = RHS.m_dblValue;
			break;
		case Type::String:
			new(&m_szValue) decltype(m_szValue);
			m_szValue = std::move(RHS.m_szValue);
			break;
		case Type::Array:
			new(&m_vElements) decltype(m_vElements);
			m_vElements = std::move(RHS.m_vElements);
			break;
		case Type::Object:
			new(&m_mChildren) decltype(m_mChildren);
			m_mChildren = std::move(RHS.m_mChildren);
			break;
		default:
			break;
	}

	// invalidate other node
	RHS.Reset();
}

// object funcs

Node* Node::Get(const utf8string& szLabel)
{
	ENSURE_OBJECT

	auto it = m_mChildren.find(szLabel);
	return it != m_mChildren.end() ? &it->second : nullptr;
}

const Node* Node::Get(const utf8string& szLabel) const
{
	ENSURE_OBJECT

	auto it = m_mChildren.find(szLabel);
	return it != m_mChildren.end() ? &it->second : nullptr;
}

void Node::Set(const utf8string& szLabel, const Node& n)
{
	ENSURE_OBJECT

	m_mChildren[szLabel] = n;
}

bool Node::GetOrDefault(const utf8string& szLabel, bool bDefault) const
{
	const Node* n = Get(szLabel);
	if (!n || n->m_eType != Type::Boolean)
		return bDefault;

	return n->GetBool();
}

double Node::GetOrDefault(const utf8string& szLabel, double dblDefault) const
{
	const Node* n = Get(szLabel);
	if (!n || n->m_eType != Type::Number)
		return dblDefault;

	return n->GetNumber();
}

utf8string Node::GetOrDefault(const utf8string& szLabel, const utf8string& szDefault) const
{
	const Node* n = Get(szLabel);
	if (!n || n->m_eType != Type::Boolean)
		return szDefault;

	return n->GetString();
}

utf8string Node::GetOrDefault(const utf8string& szLabel, const utf8_t* szDefault) const
{
	const Node* n = Get(szLabel);
	if (!n || n->m_eType != Type::Boolean)
		return szDefault;

	return n->GetString();
}

void Node::ForEachMember(const std::function<void(const std::string&, Node&)>& callback)
{
	ENSURE_OBJECT

	for (auto it : m_mChildren)
		callback(it.first, it.second);
}

void Node::ForEachMember(const std::function<void(const std::string&, const Node&)>& callback) const
{
	ENSURE_OBJECT

	for (auto it : m_mChildren)
		callback(it.first, it.second);
}

size_t Node::GetNumMembers() const
{
	ENSURE_OBJECT

	return m_mChildren.size();
}

// array funcs

size_t Node::Length() const
{
	ENSURE_ARRAY

	return m_vElements.size();
}

Node* Node::At(size_t i)
{
	ENSURE_ARRAY

	return &m_vElements[i];
}

void Node::Append(const Node& n)
{
	ENSURE_ARRAY

	if (m_vElements.empty())
		m_eElementType = n.m_eType;

	if (n.m_eType != m_eElementType)
	{
		// incorrect element type being appended (not uniform)
		std::abort();
	}

	m_vElements.push_back(n);
}

void Node::Insert(size_t i, const Node& n)
{
	ENSURE_ARRAY

	m_vElements.insert(m_vElements.begin() + i, n);
}

void Node::Remove(size_t i)
{
	ENSURE_ARRAY

	m_vElements.erase(m_vElements.begin() + i);

	if (m_vElements.empty())
		m_eElementType = Type::Null; // contains nothing
}

Node::Type Node::GetElementType() const
{
	ENSURE_ARRAY

	return m_eElementType;
}

void Node::ForEachElement(const std::function<void(Node&)>& callback)
{
	ENSURE_ARRAY

	for (Node& n : m_vElements)
		callback(n);
}

void Node::ForEachElement(const std::function<void(const Node&)>& callback) const
{
	ENSURE_ARRAY

	for (const Node& n : m_vElements)
		callback(n);
}

void Node::Reset()
{
	switch (m_eType)
	{
		case Type::Boolean:
		case Type::Number:
			break;
		case Type::String:
			m_szValue.clear();
			m_szValue.~decltype(m_szValue)();
			break;
		case Type::Array:
			m_vElements.clear();
			m_vElements.~decltype(m_vElements)();
			break;
		case Type::Object:
			m_mChildren.clear();
			m_mChildren.~decltype(m_mChildren)();
			break;
		default:
			break;
	}

	memset(this, 0, sizeof(Node));
	m_eType = Type::Null;
}

}

#undef ON_TYPE_CHECK_FAIL
#undef ENSURE_OBJECT
#undef ENSURE_ARRAY