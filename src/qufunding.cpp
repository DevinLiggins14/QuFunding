// FILE: Qufunding.cpp
// =================================================================================================================
#include "qubic.h"
#include <cstring>

// ===================================================================
// 1. CONSTANTS & ENUMS
// ===================================================================
constexpr long long QU = 1000000;
constexpr long long MAX_SHARES = 1000000 * QU;
constexpr int SUCCESS_FEE_PERCENT = 1;
constexpr int FAILURE_FEE_PERCENT = 5;
constexpr int MAX_CAMPAIGNS = 128;
constexpr int MAX_CONTRIBUTORS_PER_CAMPAIGN = 256;
constexpr int MAX_SHAREHOLDERS = 512;
constexpr int BURN_PERCENT_OF_FEES = 50;

enum FunctionID : uint8_t {
    FUNC_INITIALIZE = 0,
    FUNC_CREATE_CAMPAIGN = 1,
    FUNC_FINALIZE_CAMPAIGN = 2,
    FUNC_CLAIM_DIVIDENDS = 3
};

enum CampaignState : uint8_t {
    STATE_ACTIVE = 0,
    STATE_SUCCESSFUL = 1,
    STATE_FAILED = 2
};

// ===================================================================
// 2. DATA STRUCTURES
// ===================================================================
#pragma pack(push, 1)

struct Contribution {
    uint8_t contributor[32];
    long long amount;
};

struct Campaign {
    uint8_t creator[32];
    uint8_t beneficiary[32];
    long long goal;
    long long raised;
    long long startEpoch;
    long long endEpoch;
    CampaignState state;
    uint16_t contributorCount;
    Contribution contributions[MAX_CONTRIBUTORS_PER_CAMPAIGN];
};

struct Shareholder {
    uint8_t identity[32];
    long long shares;
    long long lastRevenuePerShareClaimed;
};

struct ContractState {
    bool isInitialized;
    uint8_t owner[32];
    long long ipoEndEpoch;
    long long ipoSharePrice;
    long long creationFee;
    Shareholder shareholders[MAX_SHAREHOLDERS];
    uint16_t shareholderCount;
    long long totalSharesIssued;
    long long cumulativeRevenuePerShare;
    long long treasury;
    Campaign campaigns[MAX_CAMPAIGNS];
    uint16_t campaignCount;
    long long totalSharesBurned;
};

#pragma pack(pop)

// ===================================================================
// 3. HELPER FUNCTIONS
// ===================================================================
int findOrAddShareholder(const uint8_t* identity) {
    auto* state = get_state_ptr<ContractState>();
    for (int i = 0; i < state->shareholderCount; ++i) {
        if (memcmp(state->shareholders[i].identity, identity, 32) == 0) {
            return i;
        }
    }
    if (state->shareholderCount < MAX_SHAREHOLDERS) {
        int newIndex = state->shareholderCount++;
        memcpy(state->shareholders[newIndex].identity, identity, 32);
        state->shareholders[newIndex].shares = 0;
        state->shareholders[newIndex].lastRevenuePerShareClaimed = 0;
        return newIndex;
    }
    return -1;
}

void processPlatformFee(long long feeAmount) {
    auto* state = get_state_ptr<ContractState>();
    if (!state->isInitialized || feeAmount <= 0) {
        return;
    }

    long long burnAmount = (feeAmount * BURN_PERCENT_OF_FEES) / 100;
    long long dividendAmount = feeAmount - burnAmount;

    if (burnAmount > 0 && state->ipoSharePrice > 0) {
        long long sharesToBurn = (burnAmount * QU) / state->ipoSharePrice;
        if (sharesToBurn > 0 && state->totalSharesIssued > sharesToBurn) {
            state->totalSharesIssued -= sharesToBurn;
            state->totalSharesBurned += sharesToBurn;
        }
    }

    if (dividendAmount > 0) {
        state->treasury += dividendAmount;
        if (state->totalSharesIssued > 0) {
            state->cumulativeRevenuePerShare += (dividendAmount * QU) / state->totalSharesIssued;
        }
    }
}

// ===================================================================
// 4. CORE LOGIC HANDLERS
// ===================================================================
void handleInitialize(const Request& req) {
    auto* state = get_state_ptr<ContractState>();
    if (state->isInitialized || req.payload_size != 1 + 8 + 8 + 8) return;

    uint8_t* p = req.payload + 1;
    long long ipoEndEpoch = 0; memcpy(&ipoEndEpoch, p, 8); p += 8;
    long long ipoSharePrice = 0; memcpy(&ipoSharePrice, p, 8); p += 8;
    long long creationFee = 0; memcpy(&creationFee, p, 8);

    memcpy(state->owner, req.sourceIdentity, 32);
    state->ipoEndEpoch = ipoEndEpoch;
    state->ipoSharePrice = ipoSharePrice;
    state->creationFee = creationFee;
    state->isInitialized = true;
    state->totalSharesBurned = 0;
}

void handleCreateCampaign(const Request& req) {
    auto* state = get_state_ptr<ContractState>();
    if (!state->isInitialized || req.amount != state->creationFee || state->campaignCount >= MAX_CAMPAIGNS) return;
    if (req.payload_size != 1 + 32 + 8 + 8) return;

    processPlatformFee(state->creationFee);

    uint8_t* p = req.payload + 1;
    uint8_t beneficiary[32]; memcpy(beneficiary, p, 32); p += 32;
    long long goal = 0; memcpy(&goal, p, 8); p += 8;
    long long durationEpochs = 0; memcpy(&durationEpochs, p, 8);

    uint16_t campaignId = state->campaignCount++;
    Campaign* c = &state->campaigns[campaignId];

    memcpy(c->creator, req.sourceIdentity, 32);
    memcpy(c->beneficiary, beneficiary, 32);
    c->goal = goal;
    c->startEpoch = getCurrentEpoch();
    c->endEpoch = c->startEpoch + durationEpochs;
    c->raised = 0;
    c->contributorCount = 0;
    c->state = STATE_ACTIVE;
}

void handleFinalizeCampaign(const Request& req) {
    auto* state = get_state_ptr<ContractState>();
    if (!state->isInitialized || req.amount != 0 || req.payload_size != 1 + 2) return;

    uint16_t campaignId = 0; memcpy(&campaignId, req.payload + 1, 2);
    if (campaignId >= state->campaignCount) return;

    Campaign* c = &state->campaigns[campaignId];
    if (c->state != STATE_ACTIVE || getCurrentEpoch() < c->endEpoch) return;

    if (c->raised >= c->goal) {
        c->state = STATE_SUCCESSFUL;
        long long fee = (c->raised * SUCCESS_FEE_PERCENT) / 100;
        processPlatformFee(fee);
        transfer(c->beneficiary, c->raised - fee);
    } else {
        c->state = STATE_FAILED;
        long long fee = (c->raised * FAILURE_FEE_PERCENT) / 100;
        processPlatformFee(fee);
        long long refundPool = c->raised - fee;
        if (refundPool > 0 && c->raised > 0) {
            for (int i = 0; i < c->contributorCount; ++i) {
                long long refund = (refundPool * c->contributions[i].amount) / c->raised;
                if (refund > 0) {
                    transfer(c->contributions[i].contributor, refund);
                }
            }
        }
    }
}

void handleIPO(const Request& req) {
    auto* state = get_state_ptr<ContractState>();
    if (!state->isInitialized || getCurrentEpoch() >= state->ipoEndEpoch || req.amount <= 0 || state->ipoSharePrice <= 0) return;

    long long sharesToIssue = (req.amount * QU) / state->ipoSharePrice;
    if (state->totalSharesIssued + sharesToIssue > MAX_SHARES) return;

    int shareholderIndex = findOrAddShareholder(req.sourceIdentity);
    if (shareholderIndex == -1) return;

    state->shareholders[shareholderIndex].shares += sharesToIssue;
    state->totalSharesIssued += sharesToIssue;
}

void handleContribution(const Request& req) {
    auto* state = get_state_ptr<ContractState>();
    if (!state->isInitialized || req.amount <= 0 || req.payload_size != 2) return;

    uint16_t campaignId = 0; memcpy(&campaignId, req.payload, 2);
    if (campaignId >= state->campaignCount) return;

    Campaign* campaign = &state->campaigns[campaignId];
    if (campaign->state != STATE_ACTIVE || getCurrentEpoch() >= campaign->endEpoch || campaign->contributorCount >= MAX_CONTRIBUTORS_PER_CAMPAIGN) return;

    int contributorIndex = -1;
    for (int i = 0; i < campaign->contributorCount; ++i) {
        if (memcmp(campaign->contributions[i].contributor, req.sourceIdentity, 32) == 0) {
            contributorIndex = i;
            break;
        }
    }
    if (contributorIndex == -1) {
        contributorIndex = campaign->contributorCount++;
        memcpy(campaign->contributions[contributorIndex].contributor, req.sourceIdentity, 32);
        campaign->contributions[contributorIndex].amount = 0;
    }

    campaign->contributions[contributorIndex].amount += req.amount;
    campaign->raised += req.amount;
}

void handleClaimDividends(const Request& req) {
    auto* state = get_state_ptr<ContractState>();
    if (!state->isInitialized || req.amount != 0 || req.payload_size != 1) return;

    int idx = findOrAddShareholder(req.sourceIdentity);
    if (idx == -1) return;

    Shareholder* s = &state->shareholders[idx];
    long long owedPerShare = state->cumulativeRevenuePerShare - s->lastRevenuePerShareClaimed;

    if (owedPerShare > 0 && s->shares > 0) {
        long long claim = (owedPerShare * s->shares) / QU;
        if (state->treasury >= claim && claim > 0) {
            state->treasury -= claim;
            s->lastRevenuePerShareClaimed = state->cumulativeRevenuePerShare;
            transfer(s->identity, claim);
        }
    }
}

// ===================================================================
// 5. MAIN ENTRY POINT
// ===================================================================
__attribute__((export_name("qubic_main")))
void qubic_main() {
    Request req;
    read_request(&req);
    auto* state = get_state_ptr<ContractState>();

    if (!state->isInitialized) {
        if (req.payload_size > 0 && req.payload[0] == FUNC_INITIALIZE) {
            handleInitialize(req);
        }
        return;
    }

    if (req.payload_size > 0) {
        uint8_t functionId = req.payload[0];
        switch (functionId) {
            case FUNC_CREATE_CAMPAIGN:
                handleCreateCampaign(req);
                return;
            case FUNC_FINALIZE_CAMPAIGN:
                handleFinalizeCampaign(req);
                return;
            case FUNC_CLAIM_DIVIDENDS:
                handleClaimDividends(req);
                return;
        }
    }

    if (req.amount > 0) {
        if (req.payload_size == 0) {
            handleIPO(req);
        } else if (req.payload_size == 2) {
            handleContribution(req);
        }
    }
}
