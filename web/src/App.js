import React, { useState } from 'react';
import './App.css';
import Header from './components/header'
import SynthesizePage from './components/synthesize_page'
import FlexContainer from './components/flex_container';

function App() {
  const [wallet, setWallet] = useState(undefined);

  return (
    <div className="App">
      <Header wallet={wallet} setWallet={setWallet}/>
      <FlexContainer>
         <SynthesizePage wallet={wallet} setWallet={setWallet}/>
     </FlexContainer>
    </div>
  );
}

export default App;
