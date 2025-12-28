#pragma once
#include "DirectXCommon.h"

class ModelCommon{
public:
	/// <summary>
	/// èâä˙âª
	/// </summary>
	void Initialize(DirectXCommon* dxCommon);

	DirectXCommon* GetDxCommon() const{ return dxCommon_; }

private:
	DirectXCommon* dxCommon_;

  
};

