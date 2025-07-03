#!/bin/bash

SYNTHFI=./synthfi-client
SOLANA=solana
SPL_TOKEN=spl-token
SOL_OPT="-u localhost --commitment finalized"
RPC="-r localhost -p 8899"

check()
{
  CMD=$1
  echo "$CMD"
  eval $CMD
  RC=$?
  if [ $RC -ne 0 ]; then
    echo "unexpected error executing rc= $RC"
    exit 1
  fi
}

setup_admin()
{
  DIR=$1
  OPT="$RPC"

  # create publisher key and fund it
  check "$SYNTHFI init_key -k $DIR"
  check "$SOLANA airdrop 10 -k $DIR/publish_key_pair.json $SOL_OPT"

  check "$SYNTHFI init_usdt -k $DIR"
  check "$SPL_TOKEN create-token $DIR/usdt_testnet_key_pair.json --fee-payer $DIR/publish_key_pair.json"
  # create program key
  check "$SYNTHFI init_program -k $DIR"

  # setup program key and deploy
  check "$SOLANA program deploy ../target/synthesizer.so -k $DIR/publish_key_pair.json --program-id $DIR/synthfi_program_key_pair.json $SOL_OPT"

  # create program key
  check "$SYNTHFI init_synthfi -k $DIR $OPT"

  # create a synthfi product account
  check "$SYNTHFI init_product -k $DIR $OPT"
}

# create key_store directory and initialize key, program and mapping
# accounts assuming you have a local build of solana and are running
# both a solana-validator and solana-faucet on localhost
# run from build/ directory
KDIR=$1
if [ -z "$KDIR" ] ; then
  echo "setup_local.sh <key_store_directory>"
  exit 1
fi
setup_admin $KDIR
