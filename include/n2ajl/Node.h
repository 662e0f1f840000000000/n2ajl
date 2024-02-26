#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>
#include "UTF.h"

namespace n2ajl
{

using utf8string = std::basic_string<utf8_t>;

class Node
{
public:
	enum class Type : uint_fast8_t
	{
		Null,
		Boolean,
		Number,
		String,
		Array,
		Object
	};

	Node();
	~Node();

	static Node Object();
	static Node Array();
	static Node String();

	bool GetBool() const;
	double GetNumber() const;
	const utf8string& GetString() const;

	// object functions
	Node* Get(const utf8string& szLabel);
	const Node* Get(const utf8string& szLabel) const;
	void Set(const utf8string& szLabel, const Node& n);
	bool GetOrDefault(const utf8string&, bool bDefault) const;
	double GetOrDefault(const utf8string&, double dblDefault) const;
	std::string GetOrDefault(const utf8string&, const utf8string& szDefault) const;
	std::string GetOrDefault(const utf8string&, const utf8_t* szDefault) const;
	void ForEachMember(const std::function<void(const std::string&, Node&)>& callback);
	void ForEachMember(const std::function<void(const std::string&, const Node&)>& callback) const;
	size_t GetNumMembers() const;

	// array functions
	size_t Length() const;
	Node* At(size_t i);
	void Append(const Node& n);
	void Insert(size_t i, const Node& n);
	void Remove(size_t i);
	Type GetElementType() const;
	void ForEachElement(const std::function<void(Node&)>& callback);
	void ForEachElement(const std::function<void(const Node&)>& callback) const;

	inline Type GetType() const { return m_eType; }

	explicit Node(bool bValue);
	Node(double dblValue);
	explicit Node(const utf8_t* szValue);
	explicit Node(const utf8string& szValue);

	Node& operator=(const Node& RHS);
	Node(const Node& RHS) noexcept;
	Node(Node&& RHS) noexcept;

private:
	union
	{
		bool m_bValue;
		double m_dblValue;
		utf8string m_szValue;
		std::vector<Node> m_vElements;
		std::map<utf8string, Node> m_mChildren;
	};

	void Reset();

	Type m_eType = Type::Null;
	Type m_eElementType = Type::Null;
};

}