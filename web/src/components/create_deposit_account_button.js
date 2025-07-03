import React from 'react'
import { Button } from 'react-bootstrap';
import { getCreateSynthfiDepositAccount } from './solana_client';
import '../styles/create_deposit_account_button.css';


class CreateDepositAccountButton extends React.Component {
    successCallback = async result => {
        let { signature, publicKey } = await this.props.wallet.sign(result.value.data, result.value.display);
        alert(`Successfully created deposit account`);
    }

    errorCallback = error => {
        console.log(`Received an error with code ${error.code} and message ${error.message}`);
        alert(`Error creating deposit account`);
    }

    onCreateDepositAccount = (syntheticProduct) => {
        let publicKey = this.props.wallet.publicKey.toBase58();
        let syntheticProductName = syntheticProduct.name;
        getCreateSynthfiDepositAccount(publicKey, syntheticProductName, this.successCallback, this.errorCallback);
    }

    render() {
        return (
            <Button id="synthesize-button" onClick={() => this.onCreateDepositAccount(this.props.syntheticProduct)}>Create Deposit Account</Button>
        );
    }
}

export default CreateDepositAccountButton
