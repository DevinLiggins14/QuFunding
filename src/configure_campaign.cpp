// FILE: configure_campaign.cpp
// PURPOSE: The client-side helper tool. 
// =================================================================================================================
#include <iostream>
#include <string>
#include <vector>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <cstring>
#include <stdexcept>
#include <algorithm>

// This function converts a byte array to a hex string for the command line. No changes needed.
std::string bytes_to_hex_string(const uint8_t* bytes, size_t size) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (size_t i = 0; i < size; ++i) {
        ss << std::setw(2) << static_cast<int>(bytes[i]);
    }
    return ss.str();
}

// This function correctly decodes a Qubic ID.
bool base26_to_bytes(const std::string& id, uint8_t* out) {
    if (id.length() != 56) return false;
    const char* alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    uint8_t buffer[45] = {0};
    for (int i = 0; i < 56; ++i) {
        const char* p = strchr(alphabet, id[i]);
        if (!p) return false;
        int val = p - alphabet;
        int carry = val;
        for (int j = 44; j >= 0; --j) {
            int new_val = buffer[j] * 26 + carry;
            buffer[j] = new_val % 256;
            carry = new_val / 256;
        }
    }
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

    // FIXED: Manually serialize the payload to guarantee correctness.
    std::vector<uint8_t> payload_bytes;

    // 1. Add Function ID
    payload_bytes.push_back(1); // FUNC_CREATE_CAMPAIGN

    // 2. Add Beneficiary ID
    uint8_t beneficiary_bytes[32];
    if (!base26_to_bytes(beneficiary_id_str, beneficiary_bytes)) {
        std::cerr << "Error: Invalid Beneficiary ID. Must be 56 uppercase letters (A-Z)." << std::endl;
        return 1;
    }
    payload_bytes.insert(payload_bytes.end(), beneficiary_bytes, beneficiary_bytes + 32);

    // 3. Add Goal (as 8 bytes, little-endian)
    long long goal_microqu = goal_qubic * 1000000;
    uint8_t* goal_ptr = reinterpret_cast<uint8_t*>(&goal_microqu);
    payload_bytes.insert(payload_bytes.end(), goal_ptr, goal_ptr + 8);

    // 4. Add Duration (as 8 bytes, little-endian)
    long long duration_epochs = (long long)duration_days * 2160; // 1 day = 2160 epochs
    uint8_t* duration_ptr = reinterpret_cast<uint8_t*>(&duration_epochs);
    payload_bytes.insert(payload_bytes.end(), duration_ptr, duration_ptr + 8);
    
    // Now convert the perfectly-formed byte vector to a hex string
    size_t payload_size = payload_bytes.size();
    std::string payload_hex = bytes_to_hex_string(payload_bytes.data(), payload_size);
    
    // This is just an example for the command line. The user MUST use the real fee set in the contract.
    long long creation_fee_qu = 500; 

    std::cout << "========================================================================================================" << std::endl;
    std::cout << "COPY AND PASTE THE FOLLOWING COMMAND INTO YOUR QUBIC-CLI TERMINAL TO CREATE A CAMPAIGN:" << std::endl;
    std::cout << "========================================================================================================" << std::endl;
    
    std::cout << ".\\qubic-cli.exe -seed \"YOUR_SEED_HERE\" "
              << "-sendtransaction " << contract_id << " " << creation_fee_qu << " 2 " 
              << payload_size << " " << payload_hex << std::endl;

    std::cout << "========================================================================================================" << std::endl;
    std::cout << "IMPORTANT: The amount sent (" << creation_fee_qu << " QU) MUST exactly match the creationFee set in the contract." << std::endl;

    return 0;
}
