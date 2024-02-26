#pragma once

#include <string>
#include "Node.h"

namespace n2ajl
{

struct SerializerConfig
{
	enum class Indentation
	{
		FourSpace,
		TwoSpace,
		Tab
	};

	Indentation m_eIndentation = Indentation::FourSpace;
	bool m_bFancy = false;
};

utf8string Serialize(const SerializerConfig& cfg, const Node& json);

}