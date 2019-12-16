let enhancer: (
  ~options: Extension.enhancerOptions('actionCreator), 
  ~devToolsUpdateActionCreator: ('state) => 'action, 
  ~actionSerializer: Types.customSerializer('action, 'serializedAction)=?,
  ~stateSerializer: Types.customSerializer('state, 'serializedState)=?, 
  unit) => Types.storeEnhancer('action, 'state);

let useReducer: (
  ~options: Extension.enhancerOptions('actionCreator), 
  ~devToolsUpdateActionCreator: ('state) => 'action,
  ~reducer: ('state, 'action) => 'state, 
  ~initial: 'state,
  ~actionSerializer: Types.customSerializer('action, 'serializedAction)=?,
  ~stateSerializer: Types.customSerializer('state, 'serializedState)=?,
  unit
) => ('state, 'action => unit);