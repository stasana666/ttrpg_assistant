#pragma once

#include <pf2e_engine/actions/generated_action.h>
#include <pf2e_engine/position.h>

#include <memory>
#include <typeindex>
#include <utility>
#include <vector>

class TGeneratedWorldView {
public:
    TGeneratedWorldView() = default;
    explicit TGeneratedWorldView(const TBattle* battle);

    bool IsBound() const;
    const TActionHistory* History() const;
    TGeneratedEntityView ResolveRecordEntity(const TRecordedEntityKey& entity) const;
    void Validate(const TGeneratedEntityView& entity) const;
    TPosition Position(const TGeneratedEntityView& entity) const;
    bool Contains(TPosition position) const;
    std::vector<TPosition> AdjacentPositions(TPosition from) const;
    bool CanOccupy(const TGeneratedEntityView& entity, TPosition position) const;
    int PositionX(const TGeneratedEntityView& entity) const;
    int PositionY(const TGeneratedEntityView& entity) const;
    int Team(const TGeneratedEntityView& entity) const;
    int GridDistance(const TGeneratedEntityView& first, const TGeneratedEntityView& second) const;
    bool SameTeam(const TGeneratedEntityView& first, const TGeneratedEntityView& second) const;
    bool HasLineOfEffect(const TGeneratedEntityView& first, const TGeneratedEntityView& second) const;

    template <class T>
    TGeneratedEntityBinding<T> Bind(const TPlayer& actor) const
    {
        return TGeneratedEntityBinding<T>(BindGeneratedActionEntity(actor, battle_));
    }

    template <class T>
    std::vector<TGeneratedEntityBinding<T>> Entities() const
    {
        std::vector<TGeneratedEntityBinding<T>> result;
        for (auto view : EntityViews(typeid(T))) {
            result.emplace_back(std::move(view));
        }
        return result;
    }

private:
    void RequireWorld() const;
    std::vector<TGeneratedEntityView> EntityViews(std::type_index type) const;

    const TBattle* battle_ = nullptr;
};

TGeneratedWorldView GeneratedActionWorld(const std::shared_ptr<TActionContext>& context);
