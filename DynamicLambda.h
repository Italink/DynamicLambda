#pragma once

#include "UObject/Class.h"
#include "Subsystems/EngineSubsystem.h"
#include "UObject/UObjectArray.h"
#include "DynamicLambda.generated.h"

namespace DynamicLambdaUtils {
	template<typename Tuple, std::size_t... Indices>
	Tuple ParseParams(void* Params, std::index_sequence<Indices...>) {
		char* Buffer = static_cast<char*>(Params);
		return Tuple{ (*reinterpret_cast<typename std::tuple_element<Indices, Tuple>::type*>(
			Buffer + (Indices * sizeof(typename std::tuple_element<Indices, Tuple>::type))))
			... };
	}

	template<typename T>
	struct FuncTraits;

	template<typename Ret, typename... Args>
	struct FuncTraits<Ret(Args...)> {
		using TupleType = std::tuple<Args...>;
	};

	template<typename Ret, typename... Args>
	struct FuncTraits<Ret(*)(Args...)> : FuncTraits<Ret(Args...)> {};

	template<typename Ret, typename... Args>
	struct FuncTraits<TFunction<Ret(Args...)>> : FuncTraits<Ret(Args...)> {};

	template<typename Lambda>
	struct FuncTraits : FuncTraits<decltype(&Lambda::operator())> {};

	template<typename ClassType, typename Ret, typename... Args>
	struct FuncTraits<Ret(ClassType::*)(Args...) const> : FuncTraits<Ret(Args...)> {};
};

UCLASS()
class UDynamicLambdaFunction : public UFunction {
    GENERATED_BODY()
public:
    template <typename FuncType>
    void SetupLambda(FuncType InFunc) {
        LambdaInvoker = [InFunc](void* Params) {
            using TupleType = typename DynamicLambdaUtils::FuncTraits<FuncType>::TupleType;

            constexpr size_t ArgCount = std::tuple_size<TupleType>::value;
            auto Args = DynamicLambdaUtils::ParseParams<TupleType>(Params, std::make_index_sequence<ArgCount>{});

            std::apply(InFunc, Args);
        };
    }
    void Invoke(void* Params);
protected:
    TFunction<void(void* Params)> LambdaInvoker;
};

UCLASS()
class UDynamicLambdaSubsystem : public UEngineSubsystem{
    GENERATED_BODY()
public:
    template <typename FuncType>
    FScriptDelegate CreateLambdaDynamic(const UObject* Owner, FuncType Lambda) {
        UClass* Class = GetClass();
        UDynamicLambdaFunction* NewDynamicLambda = NewObject<UDynamicLambdaFunction>(Class);
        NewDynamicLambda->SetFlags(RF_Transient);
        NewDynamicLambda->SetupLambda(Lambda);
        Class->AddFunctionToFunctionMap(NewDynamicLambda, NewDynamicLambda->GetFName());
        if (Owner) {
            RegisterDynamicLambdaFunction(Owner, NewDynamicLambda);
        }
        FScriptDelegate ScriptDelegate;
        ScriptDelegate.BindUFunction(this, NewDynamicLambda->GetFName());
        return ScriptDelegate;
    }
    void RemoveAll(const UObjectBase* Owner);
protected:
    void RegisterDynamicLambdaFunction(const UObject* Owner, UDynamicLambdaFunction* Function);

    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual void ProcessEvent(UFunction* Function, void* Parms) override;
private:
    class UDynamicLambdaSubsystemPrivate* Private = nullptr;
};

#define BindDynamicLambda(OwnerUObject, Lambda) Bind(GEngine->GetEngineSubsystem<UDynamicLambdaSubsystem>()->CreateLambdaDynamic(OwnerUObject, Lambda))
#define AddDynamicLambda(OwnerUObject, Lambda) Add(GEngine->GetEngineSubsystem<UDynamicLambdaSubsystem>()->CreateLambdaDynamic(OwnerUObject, Lambda))