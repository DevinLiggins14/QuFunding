#include <iostream>
#include <string>
#include <vector>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <cstring>
#include <stdexcept>
#include <algorithm>

// This struct MUST match the one in the smart contract
struct CreateCampaignPayload {
    uint8_t functionId;
    uint8_t beneficiary[32];
    long long goal;
    long long durationEpochs;
};

// NEW: Function to convert a hex string to a byte array
std::string bytes_to_hex_string(const uint8_t* bytes, size_t size) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (size_t i = 0; i < size; ++i) {
        ss << std::setw(2) << static_cast<int>(bytes[i]);
    }
    return ss.str();
}

// NEW: The correct base-26 decoder for Qubic IDs
bool base26_to_bytes(const std::string& id, uint8_t* out) {
    if (id.length() != 56) return false;

    const char* alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    uint8_t buffer[45] = {0}; // 56 chars * log2(26) / 8 bits ~= 44.1 bytes

    for (int i = 0; i < 56; ++i) {
        const char* p = strchr(alphabet, id[i]);
        if (!p) return false; // Invalid character
        int val = p - alphabet;

        int carry = val;
        for (int j = 44; j >= 0; --j) {
            int new_val = buffer[j] * 26 + carry;
            buffer[j] = new_val % 256;
            carry = new_val / 256;
        }
    }
    
    // The result is in the last 32 bytes of the buffer
    memcpy(out, buffer + (45 - 32), 32);
    return true;
}


int main(int argc, char* argv[]) {
    if (argc != 5) {
        std::cerr << "Usage: " << argv[0] << " <CONTRACT_ID> <BENEFICIARY_ID> <GOAL_IN_QUBIC> <DURATION_IN_DAYS>" << std::endl;
        std::cerr << "Example: " << argv[0] << " ABC...XYZ CCBMSSWAXGMFREXGYTKKQZTTZTUBNCIMGMKPNIRREDVTDFMZUXLWSLAGASPL 1000 30" << std::endl;
        return 1;
    }

    std::string contract_id = argv[1];
    std::string beneficiary_id_str = argv[2];
    long long goal_qubic = std::stoll(argv[3]);
    int duration_days = std::stoi(argv[4]);

    CreateCampaignPayload payload;
    payload.functionId = 1; // Corresponds to FUNC_CREATE_CAMPAIGN

    // NEW: Use the correct decoder instead of the broken memcpy
    if (!base26_to_bytes(beneficiary_id_str, payload.beneficiary)) {
        std::cerr << "Error: Invalid Beneficiary ID. Must be 56 uppercase letters (A-Z)." << std::endl;
        return 1;
    }

    payload.goal = goal_qubic * 1000000; // Convert Qubic to microqu
    payload.durationEpochs = (long long)duration_days * 2160; // 1 day = 2160 epochs

    size_t payload_size = sizeof(payload);
    std::string payload_hex = bytes_to_hex_string(reinterpret_cast<uint8_t*>(&payload), payload_size);

    long long creation_fee_qu = 500; // The default fee, as set during initialization

    std::cout << "========================================================================================================" << std::endl;
    std::cout << "COPY AND PASTE THE FOLLOWING COMMAND INTO YOUR QUBIC-CLI TERMINAL TO CREATE A CAMPAIGN:" << std::endl;
    std::cout << "========================================================================================================" << std::endl;
    
    std::cout << ".\\qubic-cli.exe -seed \"YOUR_SEED_HERE\" "
              << "-sendtransaction " << contract_id << " " << creation_fee_qu << " 2 " 
              << payload_size << " " << payload_hex << std::endl;

    std::cout << "========================================================================================================" << std::endl;
    std::cout << "NOTE: This assumes the contract was initialized with a creation fee of " << creation_fee_qu << " Qubic." << std::endl;

    return 0;
}
