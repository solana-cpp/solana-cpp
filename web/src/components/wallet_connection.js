import React from 'react'
import { NavDropdown } from 'react-bootstrap';
import { Wallet } from 'react-bootstrap-icons';
import connectSollet from './connect_sollet';
import '../styles/wallet_connection.css';
import solletLogo from '../assets/sollet-logo.png';


function WalletConnection(props) {
    if (!props.wallet)
    {
        return (
            <NavDropdown title={<span><Wallet/>&nbsp;&nbsp;connect wallet&nbsp;</span>} id="connect-dropdown">
                <NavDropdown.Item onClick={() => connectSollet(props.setWallet)} id="wallet-item">
                    <span><img src={solletLogo} id='wallet-logo' alt=''/>Sollet</span>
                </NavDropdown.Item>
            </NavDropdown>
        )
    }

    return (
        <NavDropdown title={props.wallet.publicKey.toBase58()} id="connect-dropdown">
            <NavDropdown.Item onClick={async () => await props.wallet.disconnect()} id="wallet-item">disconnect</NavDropdown.Item>
        </NavDropdown>
    )
}

export default WalletConnection
