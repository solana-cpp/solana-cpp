import React from 'react'
import ReactPaginate from 'react-paginate';
import NavHeader from './nav_header';
import SyntheticProductCard from './synthetic_product_card';
import { getSynthfiInfo, getSynthfiAccountInfo } from './solana_client';
import '../styles/synthesize_page.css';
import usdt from '../assets/usdt.png';

const CARDS_PER_PAGE = 5;
const INTERVAL = 1000;

class SynthesizePage extends React.Component {
    constructor() {
        super();
        this.state = {
            syntheticProducts: [],
            accountInfo: undefined,
            pageCount: 0,
            offset: 0,
            price: 0
        };
    }

    accountInfoSuccessCallback = result => {
        this.setState({accountInfo: result.value});
    }

    accountInfoErrorCallback = error => {
        console.log(`Received an error with code ${error.code} and message ${error.message}`);
        let tempData = {
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
                         };
         this.setState({accountInfo: tempData});
    };

    synthfiInfoSuccessCallback = result => {
        console.log(result);
        this.setSyntheticProducts(result.value.syntheticProducts);
    }

    synthfiInfoErrorCallback = error => {
        console.log(`Received an error with code ${error.code} and message ${error.message}`);
        let tempData = {
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
                                 "totalSynthesized": Math.random(),
                                 "price": 200000000000
                               },
                               {
                                 "name": "AMZN",
                                 "assetType": "equity",
                                 "description": "APPLE INC",
                                 "underlyingToken": "USDT",
                                 "underlyingTokenExponent": -9,
                                 "syntheticTokenExponent": -9,
                                 "totalDeposited": 150000,
                                 "totalSynthesized": 165,
                                 "price": 900000000000
                               },
                               {
                                 "name": "GOOG",
                               },
                               {
                                 "name": "FB",
                               },
                               {
                                 "name": "NFLX",
                               },
                               {
                                 "name": "SPY",
                               },
                               {
                                 "name": "ES",
                               },
                               {
                                 "name": "QQQ",
                               }
                             ],
                           "USDTprice": 1.001,
                         };

            this.setSyntheticProducts(tempData.syntheticProducts);
            this.setState({price: tempData.USDTprice});
    };

    setSyntheticProducts = (value) => {
        this.setState({syntheticProducts: value});
        this.setState({pageCount: Math.ceil(value.length / CARDS_PER_PAGE)});
    }

    componentDidMount() {
        this.interval = setInterval(() => {
            getSynthfiInfo(this.synthfiInfoSuccessCallback, this.synthfiInfoErrorCallback);
            if (this.props.wallet)
            {
                getSynthfiAccountInfo(this.props.wallet.publicKey.toBase58(), this.accountInfoSuccessCallback, this.accountInfoErrorCallback);
            }
        }, INTERVAL);
    }

     onPageChange = (data) => {
        let index = data.selected;
        let offset = CARDS_PER_PAGE * index;
        this.setState({offset: offset});
     }

    render() {
        return (
            <div id="synthesize-page">
                <div id="synthesize-header">
                    <NavHeader text="Synthesize"/>
                    <span id='usdt'><img src={usdt} alt=''/>${this.state.price}</span>
                </div>
                {
                    this.state.syntheticProducts.slice(this.state.offset, this.state.offset + CARDS_PER_PAGE).map(p =>
                        <SyntheticProductCard
                            syntheticProduct={p}
                            accountInfo={this.state.accountInfo}
                            {...this.props}/>)
                }
                <ReactPaginate
                    pageCount={this.state.pageCount}
                    pageRangeDisplayed={7}
                    marginPagesDisplayed={2}
                    onPageChange={this.onPageChange}
                    breakClassName={'page-item'}
                                             breakLinkClassName={'page-link'}
                                             containerClassName={'pagination'}
                                             pageClassName={'page-item'}
                                             pageLinkClassName={'page-link'}
                                             previousClassName={'page-item'}
                                             previousLinkClassName={'page-link'}
                                             nextClassName={'page-item'}
                                             nextLinkClassName={'page-link'}
                                             activeClassName={'active'}/>
            </div>
        )
    }
}

export default SynthesizePage
