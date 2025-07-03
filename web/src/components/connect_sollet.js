import Wallet from '@project-serum/sol-wallet-adapter';

const connectSollet = async (setWallet) => {
    let providerUrl = 'https://www.sollet.io';
    let wallet = new Wallet(providerUrl);
    wallet.on('connect', () => {
        alert(`connected`);
        setWallet(wallet);
    })
    wallet.on('disconnect', () => {
        alert(`disconnected`);
        setWallet(undefined);
    })
    await wallet.connect();
};

export default connectSollet
