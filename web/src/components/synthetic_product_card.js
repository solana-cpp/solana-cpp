import React from 'react'
import { Card, Col, Row, Accordion } from 'react-bootstrap';
import SyntheticProductAction from './synthetic_product_action';
import '../styles/synthesize_page.css';

class SyntheticProductCard extends React.Component {
    render() {
        const { syntheticProduct, accountInfo } = this.props;

        let wallet;
        if (accountInfo)
        {
            wallet = (accountInfo.syntheticProducts.find(p => p.name === syntheticProduct.name));
        }

        return (
            <Accordion>
                <Card>
                    <Accordion.Toggle as={Card.Header} eventKey="0">
                        <Row>
                            <SyntheticInfo syntheticProduct={syntheticProduct}/>
                            <SyntheticStat name="wallet" value={wallet ? wallet : '--'}/>
                            <SyntheticStat name="deposited" value={syntheticProduct.totalDeposited}/>
                            <SyntheticStat name="synthesized" value={syntheticProduct.totalSynthesized}/>
                        </Row>
                    </Accordion.Toggle>
                    <Accordion.Collapse as={Card.Body} eventKey="0">
                        <SyntheticProductAction {...this.props}/>
                    </Accordion.Collapse>
                </Card>
            </Accordion>
        )
    }
}

function SyntheticInfo({syntheticProduct}) {
    return (
        <Col className="synthetic-info" md={4}>
            <Row className="name">{syntheticProduct.name}</Row>
            <Row className="value">{syntheticProduct.description}</Row>
        </Col>
    )
}

function SyntheticStat({name, value}) {
    return (
        <Col className="synthetic-stat">
            <Row className="value">{value}</Row>
            <Row className="name">{name}</Row>
        </Col>
    )
}

export default SyntheticProductCard
