#pragma once

#include "UTF.h"
#include "Node.h"

namespace n2ajl
{

struct ParserConfig
{
	size_t m_uMaxDepth = 16;
};

struct Result
{
	bool m_bSuccess;
	std::string m_szMsg;
};

Result Parse(const ParserConfig& cfg, const utf8_t* szJson, Node& json);

}