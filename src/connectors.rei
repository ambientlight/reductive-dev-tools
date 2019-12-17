type customSerializer('a, 'b) = {
  serialize: 'a => 'b,
  deserialize: 'b => 'a
};

let enhancer: (
  ~options: Extension.enhancerOptions('actionCreator), 
  ~devToolsUpdateActionCreator: ('state) => 'action, 
  ~actionSerializer: customSerializer('action, 'serializedAction)=?,
  ~stateSerializer: customSerializer('state, 'serializedState)=?, 
  unit) => Types.storeEnhancer('action, 'state, 'action, 'state);

let useReducer: (
  ~options: Extension.enhancerOptions('actionCreator), 
  ~devToolsUpdateActionCreator: ('state) => 'action,
  ~reducer: ('state, 'action) => 'state, 
  ~initial: 'state,
  ~actionSerializer: customSerializer('action, 'serializedAction)=?,
  ~stateSerializer: customSerializer('state, 'serializedState)=?,
  unit
) => ('state, 'action => unit);