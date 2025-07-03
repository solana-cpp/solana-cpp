import React from 'react'
import { Form, Row, Col, Button, InputGroup } from 'react-bootstrap';
import { synthesizeProductRequest, redeemProductRequest } from './solana_client';

class SyntheticProductTransaction extends React.Component {
    constructor() {
        super();
        this.state = {
            synthesizeInputAmount: 0.00,
            synthesizeOutputAmount: 0.00,
            redeemInputAmount: 0.00,
            redeemOutputAmount: 0.00
        };
    }

    onSynthesizeInputAmountUpdate = (e) => {
        this.setState({synthesizeInputAmount: e.target.value}, this.updateSynthesizeForm);
    }

    updateSynthesizeForm = () => {
        const {syntheticProduct} = this.props;
        let synthesizedAmount = syntheticProduct.price * Math.pow(10, syntheticProduct.syntheticTokenExponent) * this.state.synthesizeInputAmount;
        this.setState({synthesizeOutputAmount: synthesizedAmount});
    }

    synthesize = (depositedAmount) => {
        let publicKey = this.props.wallet.publicKey.toBase58();
        synthesizeProductRequest(publicKey, this.props.syntheticProduct.name, depositedAmount, this.synthesizeSuccess, this.synthesizeFailure);
    }

    synthesizeSuccess = async result => {
        let { signature, publicKey } = await this.props.wallet.sign(result.data, result.display);
//        synthfiTransact(publicKey, signature); // TODO
//
//
//        synthisizeProductRequest(publicKey, name, amount);
    }

    synthesizeFailure = error => {
        console.log(`Received an error with code ${error.code} and message ${error.message}`);
        alert(`could not synthesize`);
    }

    onRedeemInputAmountUpdate = (e) => {
        this.setState({redeemInputAmount: e.target.value}, this.updateRedeemForm);
    }

    updateRedeemForm = () => {
        const {syntheticProduct} = this.props;
        let redeemedAmount = this.state.redeemInputAmount / (syntheticProduct.price * Math.pow(10, syntheticProduct.syntheticTokenExponent))
        this.setState({redeemOutputAmount: redeemedAmount});
    }

    withdraw = (redeemedAmount) => {
        let publicKey = this.props.wallet.publicKey.toBase58();
        redeemProductRequest(publicKey, this.props.syntheticProduct.name, redeemedAmount, this.withdrawSuccess, this.withdrawFailure);
    }

    withdrawSuccess = async result => {
        let { signature, publicKey } = await this.props.wallet.sign(result.data, result.display);
//        synthfiTransact(publicKey, signature); // TODO
//
//
//        synthisizeProductRequest(publicKey, name, amount);
    }

    withdrawFailure = error => {
        console.log(`Received an error with code ${error.code} and message ${error.message}`);
        alert(`could not withdraw`);
    }

    render() {
        return (
            <Row>
                <Col>
                    <SynthesizeForm
                        syntheticProductName={this.props.syntheticProduct.name}
                        input={this.state.synthesizeInputAmount}
                        onInputUpdate={this.onSynthesizeInputAmountUpdate}
                        output={this.state.synthesizeOutputAmount}
                        onClick={this.synthesize}/>
                </Col>
                <Col>
                    <RedeemForm
                        syntheticProductName={this.props.syntheticProduct.name}
                        input={this.state.redeemInputAmount}
                        onInputUpdate={this.onRedeemInputAmountUpdate}
                        output={this.state.redeemOutputAmount}
                        onClick={this.withdraw}/>
                </Col>
            </Row>
        )
    }
}

function SynthesizeForm({syntheticProductName, input, onInputUpdate, output, onClick}) {
    return (
        <Form>
              <InputGroup>
                <InputGroup.Prepend>
                  <InputGroup.Text>Deposit</InputGroup.Text>
                </InputGroup.Prepend>
                    <Form.Control placeholder="0.00" value={input} onChange={onInputUpdate}/>
                <InputGroup.Append>
                  <InputGroup.Text>USDT</InputGroup.Text>
                </InputGroup.Append>
              </InputGroup>

              <InputGroup>
                <InputGroup.Prepend>
                  <InputGroup.Text>Synthesize</InputGroup.Text>
                </InputGroup.Prepend>
                    <Form.Control placeholder="0.00" value={input} readOnly/>
                <InputGroup.Append>
                  <InputGroup.Text>{syntheticProductName}</InputGroup.Text>
                </InputGroup.Append>
              </InputGroup>
            <Button onClick={() => onClick(input)} disabled={!output}>Synthesize</Button>
        </Form>
    )
}

function RedeemForm({syntheticProductName, input, onInputUpdate, output, onClick}) {
    return (
        <Form>
              <InputGroup>
                <InputGroup.Prepend>
                  <InputGroup.Text>Redeem</InputGroup.Text>
                </InputGroup.Prepend>
                    <Form.Control placeholder="0.00" value={input} onChange={onInputUpdate}/>
                <InputGroup.Append>
                  <InputGroup.Text>{syntheticProductName}</InputGroup.Text>
                </InputGroup.Append>
              </InputGroup>

              <InputGroup>
                <InputGroup.Prepend>
                  <InputGroup.Text>Withdraw</InputGroup.Text>
                </InputGroup.Prepend>
                    <Form.Control placeholder="0.00" value={input} readOnly/>
                <InputGroup.Append>
                  <InputGroup.Text>USDT</InputGroup.Text>
                </InputGroup.Append>
              </InputGroup>
            <Button onClick={() => onClick(input)} disabled={!output}>Withdraw</Button>
        </Form>
    )
}

export default SyntheticProductTransaction
