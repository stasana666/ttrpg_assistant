#pragma once

#include <ttrpg/module.h>
#include <ttrpg/action_flow.h>

#include <string>
#include <vector>

struct TCompiledField {
    TFieldDecl Declaration;
    std::string CppType;
    std::string Initializer;
};

struct TCompiledMethod {
    TClassDecl::TMethod Declaration;
    std::vector<std::string> Body;
};

struct TCompiledGrant {
    enum class EKind { Definition, Value, Collection };
    EKind Kind = EKind::Definition;
    std::string DefinitionId;
    std::string Accessor;
    std::vector<std::string> Fields;
    std::string Site;
};

struct TCompiledClass {
    std::vector<TCompiledField> Fields;
    std::vector<std::size_t> InitOrder;
    std::vector<TCompiledMethod> Methods;
    std::vector<TCompiledGrant> Grants;
};

struct TCompiledActionCost {
    std::string Key;
    int Count = 0;
    std::string Read;
    std::string Write;
};

struct TCompiledAction {
    std::vector<TCompiledActionCost> Costs;
    std::vector<std::string> Body;
    std::vector<std::string> Requirement;
    TCompiledActionFlow Flow;
};

struct TCompiledActivityParameter {
    std::string CppType;
    std::string Name;
};

struct TCompiledActivity {
    std::string FunctionName;
    std::string ResultType;
    std::vector<TCompiledActivityParameter> Parameters;
    TCompiledActionFlow Flow;
    std::vector<std::string> Body;
};

struct TCompiledReaction {
    std::string FunctionName;
    std::string StateType;
    std::string ArgumentsType;
    TCompiledAction Execution;
};

TCompiledClass CompileClass(const TClassDecl& declaration, const TLoadedSchemas& loaded);
std::string CompileInstance(const TInstanceDecl& declaration, const TLoadedSchemas& loaded);
TCompiledAction CompileAction(const TAbilityDecl& declaration, const TLoadedSchemas& loaded);
TCompiledActivity CompileActivity(const TActivityDecl& declaration, const TLoadedSchemas& loaded);
TCompiledReaction CompileReaction(const TAbilityDecl& declaration, const TLoadedSchemas& loaded);
std::vector<std::string> CompileFeatureHook(const TAbilityDecl& feature, const TAbilityDecl::THook& hook,
                                          const TLoadedSchemas& loaded);
