import { JSONRPCClient } from "json-rpc-2.0";

let nextID: number = 0;
const createID = () => nextID++;

// JSONRPCClient needs to know how to send a JSON-RPC request.
// Tell it by passing a function to its constructor. The function must take a JSON-RPC request and send it.
const client = new JSONRPCClient((jsonRPCRequest) =>
    fetch("http://solana-dev:8799", {
        method: "POST",
        headers: {
            "content-type": "application/json",
        },
        body: JSON.stringify(jsonRPCRequest),
    }).then((response) => {
        console.log(response);
        if (response.status === 200) {
            // Use client.receive when you received a JSON-RPC response.
            return response
                .json()
                .then((jsonRPCResponse) => client.receive(jsonRPCResponse));
        } else if (jsonRPCRequest.id !== undefined) {
            return Promise.reject(new Error(response.statusText));
        }
    })
);

async function sendSolanaRequest(jsonRPCRequest, resultCallback, errorCallback)
{
    console.log(jsonRPCRequest);
    client
        .requestAdvanced(jsonRPCRequest)
        .then(function(jsonRPCResponse: JSONRPCResponse) {
            if (jsonRPCResponse.error) {
                errorCallback(jsonRPCResponse.error);
            } else {
                resultCallback(jsonRPCResponse.result);
            }
        }.bind(this));
};

export async function getSynthfiInfo(resultCallback, errorCallback)
{
    const jsonRPCRequest: JSONRPCRequest = {
        "jsonrpc": "2.0",
        "id": createID(),
        "method": "getSynthfiInfo",
        "params": []
    };

    sendSolanaRequest(jsonRPCRequest, resultCallback, errorCallback);
}

export async function getSynthfiAccountInfo(publicKey, resultCallback, errorCallback) {
    const jsonRPCRequest: JSONRPCRequest = {
        "jsonrpc": "2.0",
        "id": createID(),
        "method": "getSynthfiAccountInfo",
        "params": [
            publicKey
        ]
    };

    sendSolanaRequest(jsonRPCRequest, resultCallback, errorCallback);
};

export async function getCreateSynthfiDepositAccount(publicKey, syntheticProductName, resultCallback, errorCallback) {
    const jsonRPCRequest: JSONRPCRequest = {
        "jsonrpc": "2.0",
        "id": createID(),
        "method": "getSynthfiAccountInfo",
        "params": [
            publicKey,
            syntheticProductName
        ]
    };

    sendSolanaRequest(jsonRPCRequest, resultCallback, errorCallback);
};

export async function synthesizeProductRequest(publicKey, syntheticProductName, depositAmount, resultCallback, errorCallback) {
    const jsonRPCRequest: JSONRPCRequest = {
        "jsonrpc": "2.0",
        "id": createID(),
        "method": "synthesizeProductRequest",
        "params": [
            publicKey,
            syntheticProductName,
            depositAmount
        ]
    };

    sendSolanaRequest(jsonRPCRequest, resultCallback, errorCallback);
};

export async function redeemProductRequest(publicKey, syntheticProductName, redeemAmount, resultCallback, errorCallback) {
    const jsonRPCRequest: JSONRPCRequest = {
        "jsonrpc": "2.0",
        "id": createID(),
        "method": "redeemProductRequest",
        "params": [
            publicKey,
            syntheticProductName,
            redeemAmount
        ]
    };

    sendSolanaRequest(jsonRPCRequest, resultCallback, errorCallback);
};
