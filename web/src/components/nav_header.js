import React from 'react'
import '../styles/nav_header.css';

function NavHeader(props) {
    return (
        <h3 id="nav-header">{props.text}</h3>
    )
}

export default NavHeader
