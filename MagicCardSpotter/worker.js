self.importScripts('/cardspotter.js');

const Actions = {
  FIND_CARD: 'FIND_CARD',
  ADD_SCREEN: 'ADD_SCREEN',
  SET_SETTING: 'SET_SETTING'
};

let ready = false;
let CS;

// eslint-disable-next-line no-undef
CardSpotter().then(function(Module) {
  CS = new Module.CardSpotter();
  self.postMessage({status: "runtime initialized"});
  fetch('magic.db').then((response) => {
    return response.text().then((text) => {
      CS.LoadDatabase(text);
      CS.SetSetting("automatchhistorysize", "3");
      CS.SetSetting("minCardsize", "7");
      CS.SetSetting("maxCardsize", "20");
      CS.SetSetting("okscore", " 75");
      CS.SetSetting("goodscore", " 84");
      self.postMessage({status: "database installed"});
      ready = true;
    });
  });

  const findCard = (imageData, width, height, x, y, scale = 1) => {
    const uint8_t_ptr = Module._malloc(imageData.length);
    Module.HEAPU8.set(imageData, uint8_t_ptr);
    const result = CS.FindCardInImage(uint8_t_ptr, imageData.length, width, height);
    Module._free(uint8_t_ptr);
    self.postMessage({ result });
  }

  const addScreen = (imageData, width, height) => {
    const uint8_t_ptr = Module._malloc(imageData.length);
    Module.HEAPU8.set(imageData, uint8_t_ptr);
    const result = CS.AddScreen(uint8_t_ptr, imageData.length, width, height);
    Module._free(uint8_t_ptr);
    self.postMessage({ result });
  }

  self.onmessage = (event) => {
    if (!ready) {
      self.postMessage({status: "not ready"});
      return;
    }
    const action = event.data.action;
    
     if (action === Actions.FIND_CARD) {
      const { imageData, width, height, x, y, scale } = event.data;
      findCard(imageData, width, height, x, y, scale);
    }
    else if (action === Actions.ADD_SCREEN)
    {
      const { imageData, width, height } = event.data;
      addScreen(imageData, width, height);
    }
    else if (action === Actions.SET_SETTING)
    {
      const { key, value } = event.data;
      CS.SetSetting(key, value);
    }
  }
});
