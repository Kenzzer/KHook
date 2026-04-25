#include <gtest/gtest.h>
#include <iostream>
#include <khook.hpp>
#include <vector>
#include "helpers.hpp"

class IWideCallTarget
{
public:
    virtual ~IWideCallTarget() = default;
    virtual void DispatchWithManyArgs(
        void *selector,
        int targetId,
        int groupId,
        void *label,
        float scale,
        float spread,
        int flags = 0,
        int variant = 100,
        int mode = 0,
        void *primaryPoint = nullptr,
        void *secondaryPoint = nullptr,
        void *pointHistory = nullptr,
        bool refreshState = true,
        float scheduledTime = 0.0f,
        int ownerToken = -1) = 0;
};

struct WideCallArgs
{
    void *target = nullptr;
    void *selector = nullptr;
    int targetId = 0;
    int groupId = 0;
    void *label = nullptr;
    float scale = 0.0f;
    float spread = 0.0f;
    int flags = 0;
    int variant = 0;
    int mode = 0;
    void *primaryPoint = nullptr;
    void *secondaryPoint = nullptr;
    void *pointHistory = nullptr;
    bool refreshState = false;
    float scheduledTime = 0.0f;
    int ownerToken = 0;
};

struct WideCallArgsTestState
{
    WideCallArgs expectedArgs;
    int preHookCalls = 0;
    int originalCalls = 0;

    void AssertEquals(const WideCallArgs &actualArgs)
    {
        EXPECT_EQ(actualArgs.target, expectedArgs.target);
        EXPECT_EQ(actualArgs.selector, expectedArgs.selector);
        EXPECT_EQ(actualArgs.targetId, expectedArgs.targetId);
        EXPECT_EQ(actualArgs.groupId, expectedArgs.groupId);
        EXPECT_EQ(actualArgs.label, expectedArgs.label);
        EXPECT_EQ(actualArgs.scale, expectedArgs.scale);
        EXPECT_EQ(actualArgs.spread, expectedArgs.spread);
        EXPECT_EQ(actualArgs.flags, expectedArgs.flags);
        EXPECT_EQ(actualArgs.variant, expectedArgs.variant);
        EXPECT_EQ(actualArgs.mode, expectedArgs.mode);
        EXPECT_EQ(actualArgs.primaryPoint, expectedArgs.primaryPoint);
        EXPECT_EQ(actualArgs.secondaryPoint, expectedArgs.secondaryPoint);
        EXPECT_EQ(actualArgs.pointHistory, expectedArgs.pointHistory);
        EXPECT_EQ(actualArgs.refreshState, expectedArgs.refreshState);
        EXPECT_EQ(actualArgs.scheduledTime, expectedArgs.scheduledTime);
        EXPECT_EQ(actualArgs.ownerToken, expectedArgs.ownerToken);
    }
};

static WideCallArgsTestState *g_wideCallTestState = nullptr;

class WideCallTarget : public IWideCallTarget
{
public:
    void DispatchWithManyArgs(
        void *selector,
        int targetId,
        int groupId,
        void *label,
        float scale,
        float spread,
        int flags,
        int variant,
        int mode,
        void *primaryPoint,
        void *secondaryPoint,
        void *pointHistory,
        bool refreshState,
        float scheduledTime,
        int ownerToken) override
    {
        SCOPED_TRACE("original method");

        ++g_wideCallTestState->originalCalls;
        g_wideCallTestState->AssertEquals({
            this,
            selector,
            targetId,
            groupId,
            label,
            scale,
            spread,
            flags,
            variant,
            mode,
            primaryPoint,
            secondaryPoint,
            pointHistory,
            refreshState,
            scheduledTime,
            ownerToken,
        });
    }
};

using WideCallVirtualHook = KHook::Virtual<
    IWideCallTarget,
    void,
    void *,
    int,
    int,
    void *,
    float,
    float,
    int,
    int,
    int,
    void *,
    void *,
    void *,
    bool,
    float,
    int>;

static KHook::Return<void> WideCallPreValidateIgnore(
    IWideCallTarget *hookedThis,
    void *selector,
    int targetId,
    int groupId,
    void *label,
    float scale,
    float spread,
    int flags,
    int variant,
    int mode,
    void *primaryPoint,
    void *secondaryPoint,
    void *pointHistory,
    bool refreshState,
    float scheduledTime,
    int ownerToken)
{
    SCOPED_TRACE("pre-hook");

    ++g_wideCallTestState->preHookCalls;
    g_wideCallTestState->AssertEquals({
        hookedThis,
        selector,
        targetId,
        groupId,
        label,
        scale,
        spread,
        flags,
        variant,
        mode,
        primaryPoint,
        secondaryPoint,
        pointHistory,
        refreshState,
        scheduledTime,
        ownerToken,
    });
    return {KHook::Action::Ignore};
}

TEST(RegressionTests, WideVirtualCallArgumentsRemainConsistentAfterNoopPreHook)
{
    WideCallTarget target;
    IWideCallTarget *targetPtr = &target; // prevent compiler from de-virtualizing the call

    int selector = 24;
    float primaryPoint = 1.0f;
    float secondaryPoint = 3.0f;
    std::vector<float> pointHistory{8.0f, 13.0f, 21.0f};

    WideCallArgsTestState state;
    state.expectedArgs.target = targetPtr;
    state.expectedArgs.selector = &selector;
    state.expectedArgs.targetId = 17;
    state.expectedArgs.groupId = 5;
    state.expectedArgs.label = nullptr;
    state.expectedArgs.scale = 0.42f;
    state.expectedArgs.spread = 0.66f;
    state.expectedArgs.flags = 19;
    state.expectedArgs.variant = 123;
    state.expectedArgs.mode = 7;
    state.expectedArgs.primaryPoint = &primaryPoint;
    state.expectedArgs.secondaryPoint = &secondaryPoint;
    state.expectedArgs.pointHistory = &pointHistory;
    state.expectedArgs.refreshState = false;
    state.expectedArgs.scheduledTime = 3.5f;
    state.expectedArgs.ownerToken = 1337;

    g_wideCallTestState = &state;

    WideCallVirtualHook hook(&IWideCallTarget::DispatchWithManyArgs, &WideCallPreValidateIgnore, nullptr);
    hook.Add(targetPtr);

    targetPtr->DispatchWithManyArgs(
        state.expectedArgs.selector,
        state.expectedArgs.targetId,
        state.expectedArgs.groupId,
        state.expectedArgs.label,
        state.expectedArgs.scale,
        state.expectedArgs.spread,
        state.expectedArgs.flags,
        state.expectedArgs.variant,
        state.expectedArgs.mode,
        state.expectedArgs.primaryPoint,
        state.expectedArgs.secondaryPoint,
        state.expectedArgs.pointHistory,
        state.expectedArgs.refreshState,
        state.expectedArgs.scheduledTime,
        state.expectedArgs.ownerToken);

    EXPECT_EQ(state.preHookCalls, 1) << "Pre-hook should run exactly once";
    EXPECT_EQ(state.originalCalls, 1)
        << "Original method should still run after Ignore";

    g_wideCallTestState = nullptr;
}