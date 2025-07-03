import React from 'react'
import { Navbar } from 'react-bootstrap';
import WalletConnection from './wallet_connection';
import FlexContainer from './flex_container';
import '../styles/header.css';


function Header(props) {
    return (
        <Navbar id="header">
            <FlexContainer>
                <Navbar.Brand className="mr-auto" id="synthfi">synthfi</Navbar.Brand>
                <WalletConnection className="ml-auto" {...props}/>
            </FlexContainer>
        </Navbar>
    )
}

export default Header
