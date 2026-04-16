// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/ECACommand.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "HAL/PlatformTime.h"

//------------------------------------------------------------------------------
// Event Queue - Singleton for queuing events from editor callbacks
//------------------------------------------------------------------------------

class FECAEventQueue
{
public:
	/** Maximum number of events to keep in the queue. Older events are dropped when limit is exceeded. */
	static constexpr int32 MaxQueueSize = 10;
	
	/** Seconds to wait after startup before accepting events (avoids flood during editor load) */
	static constexpr double StartupIgnoreSeconds = 5.0;
	
	static FECAEventQueue& Get()
	{
		static FECAEventQueue Instance;
		return Instance;
	}

	/** Add an event to the queue */
	void EnqueueEvent(const FString& EventType, const FString& Message, TSharedPtr<FJsonObject> Data = nullptr, bool bTriggerAIResponse = true)
	{
		// Ignore events during startup to avoid flood from editor loading
		if (FPlatformTime::Seconds() - StartupTime < StartupIgnoreSeconds)
		{
			return;
		}
		
		FScopeLock Lock(&QueueLock);
		
		TSharedPtr<FJsonObject> Event = MakeShared<FJsonObject>();
		Event->SetStringField(TEXT("type"), EventType);
		Event->SetStringField(TEXT("message"), Message);
		Event->SetNumberField(TEXT("timestamp"), FPlatformTime::Seconds());
		Event->SetBoolField(TEXT("trigger_ai_response"), bTriggerAIResponse);
		
		if (Data.IsValid())
		{
			Event->SetObjectField(TEXT("data"), Data);
		}
		
		EventQueue.Add(Event);
		
		// If queue is too large, remove oldest events (from the front)
		if (EventQueue.Num() > MaxQueueSize)
		{
			int32 NumToRemove = EventQueue.Num() - MaxQueueSize;
			EventQueue.RemoveAt(0, NumToRemove);
			UE_LOG(LogTemp, Log, TEXT("[ECABridge] Event queue overflow - dropped %d oldest events"), NumToRemove);
		}
		
		UE_LOG(LogTemp, Log, TEXT("[ECABridge] Event queued: %s - %s"), *EventType, *Message);
	}

	/** Get and clear all pending events */
	TArray<TSharedPtr<FJsonObject>> DequeueAll()
	{
		FScopeLock Lock(&QueueLock);
		
		TArray<TSharedPtr<FJsonObject>> Events = MoveTemp(EventQueue);
		EventQueue.Empty();
		
		return Events;
	}

	/** Check if there are pending events */
	bool HasPendingEvents() const
	{
		FScopeLock Lock(&QueueLock);
		return EventQueue.Num() > 0;
	}

	/** Get the number of pending events */
	int32 GetPendingCount() const
	{
		FScopeLock Lock(&QueueLock);
		return EventQueue.Num();
	}

private:
	FECAEventQueue()
		: StartupTime(FPlatformTime::Seconds())
	{
	}
	
	TArray<TSharedPtr<FJsonObject>> EventQueue;
	mutable FCriticalSection QueueLock;
	double StartupTime;
};

//------------------------------------------------------------------------------
// get_pending_events - Polled by ECA to retrieve queued events
//------------------------------------------------------------------------------

class FECACommand_GetPendingEvents : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_pending_events"); }
	virtual FString GetDescription() const override 
	{ 
		return TEXT("Get and clear all pending events from ECABridge. Called by ECA to poll for editor events like selection changes, asset imports, etc."); 
	}
	virtual FString GetCategory() const override { return TEXT("Events"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return TArray<FECACommandParam>();  // No parameters
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override
	{
		TArray<TSharedPtr<FJsonObject>> Events = FECAEventQueue::Get().DequeueAll();
		
		TSharedPtr<FJsonObject> Result = MakeResult();
		
		// Convert events to JSON array
		TArray<TSharedPtr<FJsonValue>> EventsArray;
		for (const TSharedPtr<FJsonObject>& Event : Events)
		{
			EventsArray.Add(MakeShared<FJsonValueObject>(Event));
		}
		
		Result->SetArrayField(TEXT("events"), EventsArray);
		Result->SetStringField(TEXT("source"), TEXT("eca_bridge"));
		Result->SetNumberField(TEXT("timestamp"), FPlatformTime::Seconds());
		Result->SetNumberField(TEXT("count"), Events.Num());
		
		if (Events.Num() > 0)
		{
			UE_LOG(LogTemp, Log, TEXT("[ECABridge] Returned %d pending events"), Events.Num());
		}
		
		return FECACommandResult::Success(Result);
	}
};

//------------------------------------------------------------------------------
// queue_event - Manually queue an event (for testing or programmatic use)
//------------------------------------------------------------------------------

class FECACommand_QueueEvent : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("queue_event"); }
	virtual FString GetDescription() const override 
	{ 
		return TEXT("Manually queue an event to be sent to ECA. Useful for testing or programmatic event injection."); 
	}
	virtual FString GetCategory() const override { return TEXT("Events"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("event_type"), TEXT("string"), TEXT("Type of event (e.g., 'selection_changed', 'asset_imported')"), true },
			{ TEXT("message"), TEXT("string"), TEXT("Human-readable description of the event"), true },
			{ TEXT("data"), TEXT("object"), TEXT("Optional additional data for the event"), false },
			{ TEXT("trigger_ai_response"), TEXT("boolean"), TEXT("Whether the AI should respond to this event (default: true)"), false }
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override
	{
		FString EventType;
		if (!GetStringParam(Params, TEXT("event_type"), EventType))
		{
			return FECACommandResult::Error(TEXT("Missing required parameter: event_type"));
		}
		
		FString Message;
		if (!GetStringParam(Params, TEXT("message"), Message))
		{
			return FECACommandResult::Error(TEXT("Missing required parameter: message"));
		}
		
		bool bTriggerAIResponse = true;
		GetBoolParam(Params, TEXT("trigger_ai_response"), bTriggerAIResponse, false);
		
		TSharedPtr<FJsonObject> Data;
		const TSharedPtr<FJsonObject>* DataPtr;
		if (GetObjectParam(Params, TEXT("data"), DataPtr, false))
		{
			Data = *DataPtr;
		}
		
		FECAEventQueue::Get().EnqueueEvent(EventType, Message, Data, bTriggerAIResponse);
		
		TSharedPtr<FJsonObject> Result = MakeResult();
		Result->SetStringField(TEXT("event_type"), EventType);
		Result->SetStringField(TEXT("message"), Message);
		Result->SetBoolField(TEXT("queued"), true);
		Result->SetNumberField(TEXT("pending_count"), FECAEventQueue::Get().GetPendingCount());
		
		return FECACommandResult::Success(Result);
	}
};

//------------------------------------------------------------------------------
// get_event_queue_status - Check the event queue status
//------------------------------------------------------------------------------

class FECACommand_GetEventQueueStatus : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("get_event_queue_status"); }
	virtual FString GetDescription() const override 
	{ 
		return TEXT("Get the current status of the event queue without clearing it."); 
	}
	virtual FString GetCategory() const override { return TEXT("Events"); }
	
	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return TArray<FECACommandParam>();  // No parameters
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override
	{
		TSharedPtr<FJsonObject> Result = MakeResult();
		Result->SetBoolField(TEXT("has_pending_events"), FECAEventQueue::Get().HasPendingEvents());
		Result->SetNumberField(TEXT("pending_count"), FECAEventQueue::Get().GetPendingCount());
		Result->SetStringField(TEXT("source"), TEXT("eca_bridge"));
		
		return FECACommandResult::Success(Result);
	}
};

//------------------------------------------------------------------------------
// Register commands
//------------------------------------------------------------------------------

REGISTER_ECA_COMMAND(FECACommand_GetPendingEvents)
REGISTER_ECA_COMMAND(FECACommand_QueueEvent)
REGISTER_ECA_COMMAND(FECACommand_GetEventQueueStatus)


//------------------------------------------------------------------------------
// Helper functions for other modules to queue events
//------------------------------------------------------------------------------

namespace ECAEvents
{
	ECABRIDGE_API void QueueSelectionChanged(const FString& ActorName, const FString& ActorPath)
	{
		TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("actor_name"), ActorName);
		Data->SetStringField(TEXT("actor_path"), ActorPath);
		
		FString Message = FString::Printf(TEXT("User selected '%s' in the Unreal Editor"), *ActorName);
		FECAEventQueue::Get().EnqueueEvent(TEXT("selection_changed"), Message, Data, true);
	}

	ECABRIDGE_API void QueueAssetImported(const FString& AssetPath, const FString& AssetType)
	{
		TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("asset_path"), AssetPath);
		Data->SetStringField(TEXT("asset_type"), AssetType);
		
		FString Message = FString::Printf(TEXT("Asset imported: %s (%s)"), *AssetPath, *AssetType);
		FECAEventQueue::Get().EnqueueEvent(TEXT("asset_imported"), Message, Data, false);
	}

	ECABRIDGE_API void QueueAssetSaved(const FString& AssetPath)
	{
		TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("asset_path"), AssetPath);
		
		FString Message = FString::Printf(TEXT("Asset saved: %s"), *AssetPath);
		FECAEventQueue::Get().EnqueueEvent(TEXT("asset_saved"), Message, Data, false);
	}

	ECABRIDGE_API void QueueBlueprintCompiled(const FString& BlueprintPath, bool bSuccess, const FString& ErrorMessage)
	{
		TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("blueprint_path"), BlueprintPath);
		Data->SetBoolField(TEXT("success"), bSuccess);
		if (!bSuccess)
		{
			Data->SetStringField(TEXT("error"), ErrorMessage);
		}
		
		FString Message = bSuccess 
			? FString::Printf(TEXT("Blueprint compiled successfully: %s"), *BlueprintPath)
			: FString::Printf(TEXT("Blueprint compilation failed: %s - %s"), *BlueprintPath, *ErrorMessage);
		
		FECAEventQueue::Get().EnqueueEvent(TEXT("blueprint_compiled"), Message, Data, !bSuccess);  // Only trigger AI on failures
	}

	ECABRIDGE_API void QueueCustomEvent(const FString& EventType, const FString& Message, TSharedPtr<FJsonObject> Data, bool bTriggerAIResponse)
	{
		FECAEventQueue::Get().EnqueueEvent(EventType, Message, Data, bTriggerAIResponse);
	}
}
