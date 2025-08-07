<table>
  <tr>
    <td><h1>QuFunding: Decentralized Crowdfunding on Qubic</h1></td>
    <td>
      <p align="right">
        <img src="https://qubic.org/assets/images/qubic-logo-white-256.png" alt="Qubic" width="70" height="70"/>
        <img src="https://raw.githubusercontent.com/devicons/devicon/master/icons/cplusplus/cplusplus-original.svg" alt="C++" width="60" height="60"/>
        <img src="https://cdn-icons-png.flaticon.com/512/2897/2897825.png" alt="Smart Contract" width="60" height="60"/>
      </p>
    </td>
  </tr>
</table>

## Description
<br/>
**QuFunding** is a fully on-chain, decentralized crowdfunding platform built with C++ as a smart contract on the **Qubic network**. It empowers creators and innovators within the ecosystem to raise capital for their projects in a trustless and transparent manner.
<br/>
<br/>
Beyond simple crowdfunding, QuFunding introduces a sophisticated economic model designed to benefit the entire community. It operates as a Decentralized Autonomous Organization (DAO), featuring an Initial Platform Offering (IPO) where users can become shareholders. A percentage of all platform fees is used to systematically **burn** shares, creating a deflationary pressure that increases the value of all remaining shares over time. The rest of the fees are distributed as dividends to shareholders, creating a self-sustaining ecosystem that rewards its early supporters and participants.
<br/>
<br/>
Project Architecture:
<br/>
*(Note: For the diagram below to appear, you must have the `qufunding-architecture.svg` file from our previous discussion saved inside an `assets` folder in your repository.)*
<br/>
<img src="./assets/qufunding-architecture.svg" alt="QuFunding Architecture Diagram"/>
<br/>

## Core Features & Ecosystem Benefits

| **Feature** | **Benefit for the Qubic Ecosystem** |
| :--- | :--- |
| **Decentralized Crowdfunding** | Provides a trustless, transparent, and automated way for new projects to raise capital directly on-chain, fostering innovation and growth without intermediaries. |
| **DAO & IPO Model** | Creates a true community-owned platform. By holding shares, users invest in the platform's success and are rewarded with passive income through dividends. |
| **Deflationary Share Burn** | Introduces a powerful economic incentive for long-term holding. As the platform is used, its own shares become scarcer, systematically increasing their underlying value. |
| **Automated & Secure Payouts** | Eliminates counterparty risk. Funds are locked in the smart contract and automatically released to the creator upon success or refunded to backers upon failure. |
| **Foundation for Governance** | The shareholder structure lays the groundwork for future on-chain governance, allowing the community to vote on platform upgrades, fee structures, and other key parameters. |

<br/>

### **Prerequisites**
- A [Qubic Wallet](https://wallet.qubic.li/) to interact with the network.
- Basic understanding of [Smart Contracts](https://www.investopedia.com/terms/s/smart-contract.asp) and blockchain principles.

<br/>

## How It Works: A Look at the Code
<br/>
The logic is contained entirely within the C++ smart contract, ensuring transparency and security. Here are a few key snippets that power the platform.
<br/>
<br/>

### The Contract's State
This `struct` defines all the data stored on-chain. Note the inclusion of `totalSharesBurned` to transparently track the deflationary mechanism's impact.
```cpp
struct ContractState {
    bool isInitialized;
    uint8_t owner;
    long long ipoEndEpoch;
    long long ipoSharePrice;
    long long creationFee;
    Shareholder shareholders[MAX_SHAREHOLDERS];
    uint16_t shareholderCount;
    long long totalSharesIssued;
    long long cumulativeRevenuePerShare;
    long long treasury; // Holds funds for dividends
    Campaign campaigns[MAX_CAMPAIGNS];
    uint16_t campaignCount;
    long long totalSharesBurned; // Tracks burned shares
};
