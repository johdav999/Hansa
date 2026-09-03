#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "Templates/Function.h"

namespace Hansa::Automation
{
	enum class EHansaSemanticRole : uint8
	{
		Screen = 0,
		Panel,
		Heading,
		Status,
		Text,
		Button,
		Alert
	};

	enum class EHansaSemanticAction : uint8
	{
		Activate = 0,
		Focus
	};

	HANSAAUTOMATION_API const TCHAR* LexToString(EHansaSemanticRole Role);
	HANSAAUTOMATION_API const TCHAR* LexToString(EHansaSemanticAction Action);
	HANSAAUTOMATION_API bool TryParseSemanticAction(const FString& Text, EHansaSemanticAction& OutAction);
	HANSAAUTOMATION_API bool IsValidSemanticId(const FString& SemanticId);

	struct HANSAAUTOMATION_API FHansaSemanticBounds final
	{
		int32 X = 0;
		int32 Y = 0;
		int32 Width = 0;
		int32 Height = 0;

		friend bool operator==(const FHansaSemanticBounds& Left, const FHansaSemanticBounds& Right) = default;
	};

	struct HANSAAUTOMATION_API FHansaSemanticState final
	{
		bool bVisible = true;
		bool bEnabled = true;
		bool bFocused = false;
		bool bSelected = false;
		bool bLoading = false;
		bool bWarning = false;
		bool bError = false;
		/** Optional closed presentation type and value for non-boolean observable state. */
		FString ValueType;
		FString Value;

		friend bool operator==(const FHansaSemanticState& Left, const FHansaSemanticState& Right) = default;
	};

	struct HANSAAUTOMATION_API FHansaSemanticNode final
	{
		FString Id;
		EHansaSemanticRole Role = EHansaSemanticRole::Text;
		FString Label;
		FHansaSemanticState State;
		FHansaSemanticBounds Bounds;
		FString ParentId;
		TArray<FString> ChildIds;
		TArray<EHansaSemanticAction> Actions;
	};

	struct HANSAAUTOMATION_API FHansaSemanticActionHandlers final
	{
		TFunction<bool()> Activate;
		TFunction<bool()> Focus;
	};

	enum class EHansaSemanticRegistryError : uint8
	{
		None = 0,
		InvalidId,
		DuplicateId,
		MissingParent,
		NotFound,
		UnsupportedAction,
		ActionFailed
	};

	struct HANSAAUTOMATION_API FHansaSemanticActionResult final
	{
		EHansaSemanticRegistryError Error = EHansaSemanticRegistryError::None;
		uint64 Revision = 0;

		[[nodiscard]] bool IsSuccess() const { return Error == EHansaSemanticRegistryError::None; }
	};

	/** Widget-class-neutral semantic snapshot and action registry. */
	class HANSAAUTOMATION_API FHansaSemanticUiRegistry final
	{
	public:
		bool RegisterNode(FHansaSemanticNode Node, FHansaSemanticActionHandlers Handlers = {});
		bool UpdateNode(const FString& SemanticId, const FHansaSemanticState& State, const FHansaSemanticBounds& Bounds);
		bool SetLabel(const FString& SemanticId, const FString& Label);
		bool RemoveNode(const FString& SemanticId);
		void Reset();

		[[nodiscard]] const FHansaSemanticNode* FindNode(const FString& SemanticId) const;
		[[nodiscard]] TArray<FHansaSemanticNode> FindNodes(const FString& IdPrefix = FString()) const;
		[[nodiscard]] FHansaSemanticActionResult Invoke(const FString& SemanticId, EHansaSemanticAction Action);
		[[nodiscard]] uint64 GetRevision() const { return Revision; }

	private:
		void AdvanceRevision();

		TMap<FString, FHansaSemanticNode> Nodes;
		TMap<FString, FHansaSemanticActionHandlers> ActionHandlers;
		uint64 Revision = 0;
	};
}
