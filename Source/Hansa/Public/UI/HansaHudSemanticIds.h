#pragma once

#include "Containers/ArrayView.h"

namespace Hansa::UI
{
	/**
	 * One entry in the stable MVP HUD semantic contract. Instance prefixes receive a stable-ID
	 * suffix at runtime (for example BuildMenu.Card.Building_Bakery), never localized text or tree position.
	 */
	struct FHansaHudSemanticId final
	{
		const TCHAR* Component;
		const TCHAR* Id;
		const TCHAR* ParentId;
		bool bInstancePrefix;
	};

	HANSA_API TConstArrayView<FHansaHudSemanticId> GetMvpHudSemanticIds();
}
