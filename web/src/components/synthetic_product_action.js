import React from 'react'
import CreateDepositAccountButton from './create_deposit_account_button';
import SyntheticProductTransaction from './synthetic_product_transaction';


class SyntheticProductAction extends React.Component {
    constructor() {
        super();
        this.state = {
            status: undefined
        };
    }

    render() {
        const { accountInfo } = this.props;

        if (!accountInfo)
        {
            return <span>connect wallet to continue</span>
        }

        let account = accountInfo.syntheticProducts.find((p) => p.name === this.props.syntheticProduct.name);
        if (!account)
        {
            return <CreateDepositAccountButton {...this.props}/>
        }

        return <SyntheticProductTransaction {...this.props}/>;
    }
}

export default SyntheticProductAction
