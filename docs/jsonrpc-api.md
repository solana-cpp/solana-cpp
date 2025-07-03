---
title: synthfi client JSON RPC API
---

The synthfi client accept HTTP requests using the [JSON-RPC 2.0](https://www.jsonrpc.org/specification) specification.

## RPC HTTP Endpoint

**Default port:** 8799 eg. [http://localhost:8799](http://localhost:8799), [http://192.168.1.88:8799](http://192.168.1.88:8799)

## Methods

- [getSynthfiInfo](jsonrpc-api.md#getsynthfiinfo)
- [getSynthfiAccountInfo](jsonrpc-api.md#getsynthfiaccountinfo)
- [getCreateSynthfiDepositAccount](jsonrpc-api.md#getblock)
- [getSynthisizeProduct](jsonrpc-api.md#getblock)
- [getRedeemProduct](jsonrpc-api.md#getblock)

## Request Formatting

To make a JSON-RPC request, send an HTTP POST request with a `Content-Type:
application/json` header. The JSON request data should contain 4 fields:

- `jsonrpc: <string>`, set to `"2.0"`
- `id: <number>`, a unique client-generated identifying integer
- `method: <string>`, a string containing the method to be invoked
- `params: <array>`, a JSON array of ordered parameter values

Example using curl:

```bash
curl http://localhost:8799 -X POST -H "Content-Type: application/json" -d '
  {
    "jsonrpc": "2.0",
    "id": 1,
    "method": "getSynthfiAcccountInfo",
    "params": [
      "83astBRguLMdt2h5U1Tpdq5tjFoJ6noeGwaY3mDLVcri"
    ]
  }
'
```

The response output will be a JSON object with the following fields:

- `jsonrpc: <string>`, matching the request specification
- `id: <number>`, matching the request identifier
- `result: <array|number|object|string>`, requested data or success confirmation

Requests can be sent in batches by sending an array of JSON-RPC request objects as the data for a single POST.

## Definitions

- Hash: A SHA-256 hash of a chunk of data.
- Pubkey: The public key of a Ed25519 key-pair.
- Transaction: A list of Solana instructions signed by a client keypair to authorize those actions.
- Signature: An Ed25519 signature of transaction's payload data including instructions. This can be used to identify transactions.

#### Results:

## JSON RPC API Reference

### getSynthfiInfo:

Returns all synthfi information associated with the synthfi program

#### Parameters:

Request:
```bash
curl http://localhost:8799 -X POST -H "Content-Type: application/json" -d '
  {
    "jsonrpc": "2.0",
    "id": 1,
    "method": "getSynthfiInfo",
    "params": []
  }
'
```
Response:
```json
{
  "jsonrpc": "2.0",
  "result": {
    "syntheticProducts":
      [
        {
          "name": "AAPL",
          "assetType": "equity",
          "description": "APPLE INC",
          "underlyingToken": "USDT",
          "underlyingTokenExponent": -9,
          "syntheticTokenExponent": -9,
          "totalDeposited": 150000,
          "totalSynthesized": 165,
          "price": 200000000000
        }
      ],
    "USDTprice": 1.001,
  },
  "id": 1
}
```

### getSynthfiAccountInfo:

Returns all synthfi information associated with the account of provided Pubkey

#### Parameters:

Request:
```bash
curl http://localhost:8799 -X POST -H "Content-Type: application/json" -d '
  {
    "jsonrpc": "2.0",
    "id": 1,
    "method": "getSynthfiAccountInfo",
    "params": [
      "vines1vzrYbzLMRdu58ou5XTby4qAqVRLmqo36NKPTg"
    ]
  }
'
```
Response:
```json
{
  "jsonrpc": "2.0",
  "result": {
    "lamports": 100,
    "syntheticProducts":
      [
        {
          "name": "AAPL",
          "assetType": "equity",
          "description": "APPLE INC",
          "underlyingToken": "USDT",
          "underlyingTokenDeposit": 15000,
          "underlyingTokenExponent": -9,
          "syntheticTokenBalance": 1500,
          "syntheticTokenExponent": -9,
          "collaterializationRatio": 165
        }
      ],
    "USDTBalance": 2000000000000,
    "USDTExponent": -9,
  },
  "id": 1
}
```

### createSynthfiDepositAccount:

#### Parameters:

Request:
```bash
curl http://localhost:8799 -X POST -H "Content-Type: application/json" -d '
  {
    "jsonrpc": "2.0",
    "id": 1,
    "method": "createSynthfiDepositAccount",
    "params": [
      "vines1vzrYbzLMRdu58ou5XTby4qAqVRLmqo36NKPTg",
      "solAAPL"
    ]
  }
'
```
Response:
```json
{
  "jsonrpc": "2.0",
  "result": {
    "data" : "aaabbb",
    "display": "synthisze solAAPL"
  },
  "id": 1
}
```

### synthisizeProductRequest:

#### Parameters:

Request:
```bash
curl http://localhost:8799 -X POST -H "Content-Type: application/json" -d '
  {
    "jsonrpc": "2.0",
    "id": 1,
    "method": "synthesizeProductRequest",
    "params": [
      "vines1vzrYbzLMRdu58ou5XTby4qAqVRLmqo36NKPTg",
      "solAAPL",
      10 }
    ]
  }
'
```
Response:
```json
{
  "jsonrpc": "2.0",
  "result": {
    "data" : "aaabbb",
    "display": "synthisze solAAPL"
  },
  "id": 1
}
```

### redeemProductRequest:

#### Parameters:

Request:
```bash
curl http://localhost:8799 -X POST -H "Content-Type: application/json" -d '
  {
    "jsonrpc": "2.0",
    "id": 1,
    "method": "redeemProductRequest",
    "params": [
      "vines1vzrYbzLMRdu58ou5XTby4qAqVRLmqo36NKPTg",
      "solAAPL",
      10 }
    ]
  }
'
```
Response:
```json
{
  "jsonrpc": "2.0",
  "result": {
    "data" : "aaabbb",
    "display": "redeem solAAPL"
  },
  "id": 1
}
```
