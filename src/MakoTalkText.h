#pragma once
#include <string>
// Kana normalization only, not kanji reading or linguistic analysis.
bool mktNormalizeKana(const char* input,std::string& output,const char** error);
