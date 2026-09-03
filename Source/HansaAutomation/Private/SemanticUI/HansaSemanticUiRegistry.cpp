#include "SemanticUI/HansaSemanticUiRegistry.h"

#include "Algo/Sort.h"
#include "Algo/Unique.h"

namespace Hansa::Automation
{
	const TCHAR* LexToString(const EHansaSemanticRole Role)
	{
		switch (Role)
		{
		case EHansaSemanticRole::Screen: return TEXT("screen");
		case EHansaSemanticRole::Panel: return TEXT("panel");
		case EHansaSemanticRole::Heading: return TEXT("heading");
		case EHansaSemanticRole::Status: return TEXT("status");
		case EHansaSemanticRole::Text: return TEXT("text");
		case EHansaSemanticRole::Button: return TEXT("button");
		case EHansaSemanticRole::Alert: return TEXT("alert");
		default: return TEXT("unknown");
		}
	}

	const TCHAR* LexToString(const EHansaSemanticAction Action)
	{
		switch (Action)
		{
		case EHansaSemanticAction::Activate: return TEXT("activate");
		case EHansaSemanticAction::Focus: return TEXT("focus");
		default: return TEXT("unknown");
		}
	}

	bool TryParseSemanticAction(const FString& Text, EHansaSemanticAction& OutAction)
	{
		for (const EHansaSemanticAction Candidate : { EHansaSemanticAction::Activate, EHansaSemanticAction::Focus })
		{
			if (Text.Equals(LexToString(Candidate), ESearchCase::IgnoreCase))
			{
				OutAction = Candidate;
				return true;
			}
		}
		return false;
	}

	bool IsValidSemanticId(const FString& SemanticId)
	{
		if (SemanticId.IsEmpty() || SemanticId.Len() > 128 || !SemanticId.Contains(TEXT(".")))
		{
			return false;
		}
		for (const TCHAR Character : SemanticId)
		{
			if (!(FChar::IsAlnum(Character) || Character == TEXT('.') || Character == TEXT('_') || Character == TEXT('-')))
			{
				return false;
			}
		}
		return true;
	}

	bool FHansaSemanticUiRegistry::RegisterNode(FHansaSemanticNode Node, FHansaSemanticActionHandlers Handlers)
	{
		if (!IsValidSemanticId(Node.Id) || Nodes.Contains(Node.Id) ||
			(!Node.ParentId.IsEmpty() && !Nodes.Contains(Node.ParentId)))
		{
			return false;
		}
		Node.ChildIds.Reset();
		Node.Actions.Sort([](const EHansaSemanticAction Left, const EHansaSemanticAction Right)
		{
			return static_cast<uint8>(Left) < static_cast<uint8>(Right);
		});
		Node.Actions.SetNum(Algo::Unique(Node.Actions));
		const FString NodeId = Node.Id;
		const FString ParentId = Node.ParentId;
		Nodes.Add(NodeId, MoveTemp(Node));
		ActionHandlers.Add(NodeId, MoveTemp(Handlers));
		if (FHansaSemanticNode* Parent = Nodes.Find(ParentId))
		{
			Parent->ChildIds.Add(NodeId);
			Parent->ChildIds.Sort();
		}
		AdvanceRevision();
		return true;
	}

	bool FHansaSemanticUiRegistry::UpdateNode(
		const FString& SemanticId,
		const FHansaSemanticState& State,
		const FHansaSemanticBounds& Bounds)
	{
		FHansaSemanticNode* Node = Nodes.Find(SemanticId);
		if (Node == nullptr)
		{
			return false;
		}
		if (!(Node->State == State) || !(Node->Bounds == Bounds))
		{
			Node->State = State;
			Node->Bounds = Bounds;
			AdvanceRevision();
		}
		return true;
	}

	bool FHansaSemanticUiRegistry::SetLabel(const FString& SemanticId, const FString& Label)
	{
		FHansaSemanticNode* Node = Nodes.Find(SemanticId);
		if (Node == nullptr)
		{
			return false;
		}
		if (Node->Label != Label)
		{
			Node->Label = Label;
			AdvanceRevision();
		}
		return true;
	}

	bool FHansaSemanticUiRegistry::RemoveNode(const FString& SemanticId)
	{
		const FHansaSemanticNode* Existing = Nodes.Find(SemanticId);
		if (Existing == nullptr || !Existing->ChildIds.IsEmpty())
		{
			return false;
		}
		const FString ParentId = Existing->ParentId;
		if (FHansaSemanticNode* Parent = Nodes.Find(ParentId))
		{
			Parent->ChildIds.Remove(SemanticId);
		}
		Nodes.Remove(SemanticId);
		ActionHandlers.Remove(SemanticId);
		AdvanceRevision();
		return true;
	}

	void FHansaSemanticUiRegistry::Reset()
	{
		if (!Nodes.IsEmpty())
		{
			Nodes.Reset();
			ActionHandlers.Reset();
			AdvanceRevision();
		}
	}

	const FHansaSemanticNode* FHansaSemanticUiRegistry::FindNode(const FString& SemanticId) const
	{
		return Nodes.Find(SemanticId);
	}

	TArray<FHansaSemanticNode> FHansaSemanticUiRegistry::FindNodes(const FString& IdPrefix) const
	{
		TArray<FHansaSemanticNode> Result;
		for (const TPair<FString, FHansaSemanticNode>& Pair : Nodes)
		{
			if (IdPrefix.IsEmpty() || Pair.Key.StartsWith(IdPrefix, ESearchCase::CaseSensitive))
			{
				Result.Add(Pair.Value);
			}
		}
		Result.Sort([](const FHansaSemanticNode& Left, const FHansaSemanticNode& Right)
		{
			return Left.Id < Right.Id;
		});
		return Result;
	}

	FHansaSemanticActionResult FHansaSemanticUiRegistry::Invoke(
		const FString& SemanticId,
		const EHansaSemanticAction Action)
	{
		const FHansaSemanticNode* Node = Nodes.Find(SemanticId);
		if (Node == nullptr)
		{
			return { EHansaSemanticRegistryError::NotFound, Revision };
		}
		if (!Node->Actions.Contains(Action))
		{
			return { EHansaSemanticRegistryError::UnsupportedAction, Revision };
		}
		FHansaSemanticActionHandlers* Handlers = ActionHandlers.Find(SemanticId);
		TFunction<bool()>* Handler = nullptr;
		if (Handlers != nullptr)
		{
			Handler = Action == EHansaSemanticAction::Activate ? &Handlers->Activate : &Handlers->Focus;
		}
		if (Handler == nullptr || !*Handler || !(*Handler)())
		{
			return { EHansaSemanticRegistryError::ActionFailed, Revision };
		}
		AdvanceRevision();
		return { EHansaSemanticRegistryError::None, Revision };
	}

	void FHansaSemanticUiRegistry::AdvanceRevision()
	{
		++Revision;
		if (Revision == 0)
		{
			++Revision;
		}
	}
}
