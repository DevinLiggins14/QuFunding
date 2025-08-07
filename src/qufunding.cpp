// =================================================================================================================
// ||                                                                                                             ||
// ||                                       QuFunding - Decentralized Crowdfunding Platform                       ||
// ||                                                                                                             ||
// ||                                         Smart Contract v1                                                   ||
// ||                                                                                                             ||
// =================================================================================================================
#include "qubic.h"

// Constants
constexpr long long QU = 1000000;
constexpr long long MAX_SHARES = 1000000 * QU;
constexpr int SUCCESS_FEE_PERCENT = 1;
constexpr int FAILURE_FEE_PERCENT = 5;
constexpr int MAX_CAMPAIGNS = 128;
constexpr int MAX_CONTRIBUTORS_PER_CAMPAIGN = 256;
constexpr int MAX_SHAREHOLDERS = 512;
// BURN-FEATURE: Define what percentage of collected fees will be used to burn shares. (e.g., 50%)
constexpr int BURN_PERCENT_OF_FEES = 50;


// Function IDs
enum FunctionID : uint8_t {
    FUNC_INITIALIZE = 0,
    FUNC_CREATE_CAMPAIGN = 1,
    FUNC_FINALIZE_CAMPAIGN = 2,
    FUNC_CLAIM_DIVIDENDS = 3
};
enum CampaignState : uint8_t { STATE_ACTIVE = 0, STATE_SUCCESSFUL = 1, STATE_FAILED = 2 };

// Data Structures
struct Contribution { uint8_t contributor[32]; long long amount; };
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
struct Shareholder { uint8_t identity[32]; long long shares; long long lastRevenuePerShareClaimed; };

// ContractState now includes a variable to track burned shares
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
    long long treasury; // This now ONLY holds funds for dividends
    Campaign campaigns[MAX_CAMPAIGNS];
    uint16_t campaignCount;
    // BURN-FEATURE: Add a tracker for total shares burned.
    long long totalSharesBurned;
};

// Payloads (unchanged)
struct InitializePayload { uint8_t functionId; long long ipoEndEpoch; long long ipoSharePrice; long long creationFee; };
struct CreateCampaignPayload { uint8_t functionId; uint8_t beneficiary[32]; long long goal; long long durationEpochs; };
struct FinalizeCampaignPayload { uint8_t functionId; uint16_t campaignId; };
struct ClaimDividendsPayload { uint8_t functionId; };

// Helper Functions
int findOrAddShareholder(const uint8_t* identity) {
    auto* state = get_state_ptr<ContractState>();
    for (int i = 0; i < state->shareholderCount; ++i) {
        if (memcmp(state->shareholders[i].identity, identity, 32) == 0) return i;
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

// BURN-FEATURE: This function is now completely rewritten to handle the fee split.
void processPlatformFee(long long feeAmount) {
    auto* state = get_state_ptr<ContractState>();
    assert(state->isInitialized && feeAmount > 0);

    // 1. Calculate the split between burning and dividends
    long long burnAmount = feeAmount * BURN_PERCENT_OF_FEES / 100;
    long long dividendAmount = feeAmount - burnAmount;

    // 2. Process the burn
    if (burnAmount > 0 && state->ipoSharePrice > 0) {
        // Calculate how many shares can be "bought back" with the burn amount
        long long sharesToBurn = burnAmount * QU / state->ipoSharePrice;
        
        // Permanently remove the shares from the total supply
        if (sharesToBurn > 0 && state->totalSharesIssued > sharesToBurn) {
            state->totalSharesIssued -= sharesToBurn;
            state->totalSharesBurned += sharesToBurn;
        }
    }

    // 3. Process the dividends
    if (dividendAmount > 0) {
        // Add the dividend portion to the treasury
        state->treasury += dividendAmount;
        
        // Distribute the value of this dividend among all remaining shares
        if (state->totalSharesIssued > 0) {
            state->cumulativeRevenuePerShare += dividendAmount * QU / state->totalSharesIssued;
        }
    }
}

// The initialize function
void handleInitialize(const Request& req) {
    auto* state = get_state_ptr<ContractState>();
    assert(!state->isInitialized);
    assert(req.payload_size == sizeof(InitializePayload));

    const auto* payload = reinterpret_cast<const InitializePayload*>(req.payload);
    
    memcpy(state->owner, req.sourceIdentity, 32);
    state->ipoEndEpoch = payload->ipoEndEpoch;
    state->ipoSharePrice = payload->ipoSharePrice;
    state->creationFee = payload->creationFee;
    state->isInitialized = true;
    // BURN-FEATURE: Initialize the burn counter
    state->totalSharesBurned = 0;
}

// Core Logic (now calls processPlatformFee instead of addFeeToTreasury)
void handleCreateCampaign(const Request& req) {
    auto* state = get_state_ptr<ContractState>();
    assert(state->isInitialized);
    assert(req.amount == state->creationFee && req.payload_size == sizeof(CreateCampaignPayload));
    assert(state->campaignCount < MAX_CAMPAIGNS);
    
    // Process the fee, which will now be split between burn and dividends
    processPlatformFee(state->creationFee);
    
    const auto* payload = reinterpret_cast<const CreateCampaignPayload*>(req.payload);
    uint16_t campaignId = state->campaignCount++;
    Campaign* c = &state->campaigns[campaignId];
    
    memcpy(c->creator, req.sourceIdentity, 32);
    memcpy(c->beneficiary, payload->beneficiary, 32);
    c->goal = payload->goal;
    c->startEpoch = getCurrentEpoch();
    c->endEpoch = c->startEpoch + payload->durationEpochs;
    c->raised = 0;
    c->contributorCount = 0;
    c->state = STATE_ACTIVE;
}

void handleFinalizeCampaign(const Request& req) {
    assert(req.amount == 0 && req.payload_size == sizeof(FinalizeCampaignPayload));
    const auto* payload = reinterpret_cast<const FinalizeCampaignPayload*>(req.payload);
    auto* state = get_state_ptr<ContractState>();
    assert(state->isInitialized);
    assert(payload->campaignId < state->campaignCount);
    
    Campaign* c = &state->campaigns[payload->campaignId];
    assert(c->state == STATE_ACTIVE && getCurrentEpoch() >= c->endEpoch);
    
    if (c->raised >= c->goal) {
        c->state = STATE_SUCCESSFUL;
        long long fee = c->raised * SUCCESS_FEE_PERCENT / 100;
        processPlatformFee(fee); // Process success fee
        transfer(c->beneficiary, c->raised - fee);
    } else {
        c->state = STATE_FAILED;
        long long fee = c->raised * FAILURE_FEE_PERCENT / 100;
        processPlatformFee(fee); // Process failure fee
        long long refundPool = c->raised - fee;
        if (refundPool > 0) {
            for (int i = 0; i < c->contributorCount; ++i) {
                long long refund = refundPool * c->contributions[i].amount / c->raised;
                transfer(c->contributions[i].contributor, refund);
            }
        }
    }
}

// IPO, Contribution, and Dividend Claiming logic remains largely the same,
// as the burn complexity is handled by the new `processPlatformFee` function.
// (The rest of the functions from the previous version go here, unchanged)

void handleIPO(const Request& req) {
    auto* state = get_state_ptr<ContractState>();
    assert(state->isInitialized);
    assert(getCurrentEpoch() < state->ipoEndEpoch && req.amount > 0);
    long long sharesToIssue = req.amount * QU / state->ipoSharePrice;
    assert(state->totalSharesIssued + sharesToIssue <= MAX_SHARES);
    int shareholderIndex = findOrAddShareholder(req.sourceIdentity);
    assert(shareholderIndex != -1);
    state->shareholders[shareholderIndex].shares += sharesToIssue;
    state->totalSharesIssued += sharesToIssue;
}

void handleContribution(const Request& req) {
    assert(req.amount > 0 && req.payload_size == sizeof(uint16_t));
    uint16_t campaignId = *reinterpret_cast<const uint16_t*>(req.payload);
    auto* state = get_state_ptr<ContractState>();
    assert(state->isInitialized);
    assert(campaignId < state->campaignCount);
    Campaign* campaign = &state->campaigns[campaignId];
    assert(campaign->state == STATE_ACTIVE && getCurrentEpoch() < campaign->endEpoch && campaign->contributorCount < MAX_CONTRIBUTORS_PER_CAMPAIGN);
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
    assert(req.amount == 0 && req.payload_size == sizeof(ClaimDividendsPayload));
    auto* state = get_state_ptr<ContractState>();
    assert(state->isInitialized);
    int idx = findOrAddShareholder(req.sourceIdentity);
    assert(idx != -1);
    Shareholder* s = &state->shareholders[idx];
    long long owedPerShare = state->cumulativeRevenuePerShare - s->lastRevenuePerShareClaimed;
    if (owedPerShare > 0 && s->shares > 0) {
        long long claim = owedPerShare * s->shares / QU;
        assert(state->treasury >= claim);
        state->treasury -= claim;
        s->lastRevenuePerShareClaimed = state->cumulativeRevenuePerShare;
        transfer(s->identity, claim);
    }
}


__attribute__((export_name("qubic_main")))
void qubic_main() {
    Request req;
    read_request(&req);
    auto* state = get_state_ptr<ContractState>();

    if (!state->isInitialized) {
        handleInitialize(req);
        return;
    }

    if (req.amount > 0) {
        if (req.payload_size == 0) handleIPO(req);
        else handleContribution(req);
    } else {
        assert(req.payload_size > 0);
        uint8_t functionId = req.payload[0];
        if (functionId == FUNC_CREATE_CAMPAIGN) handleCreateCampaign(req);
        else if (functionId == FUNC_FINALIZE_CAMPAIGN) handleFinalizeCampaign(req);
        else if (functionId == FUNC_CLAIM_DIVIDENDS) handleClaimDividends(req);
        else assert(false);
    }
}
