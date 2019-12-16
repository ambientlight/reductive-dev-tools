let nextEnhancer: (
  ~options: Extension.enhancerOptions('actionCreator), 
  ~devToolsUpdateActionCreator: ('state) => 'action, 
  ~actionSerializer: Types.customSerializer('action, 'serializedAction)=?,
  ~stateSerializer: Types.customSerializer('state, 'serializedState)=?, 
  unit) => Types.storeEnhancer('action, 'state);

let useNextReducer: (
  ~options: Extension.enhancerOptions('actionCreator), 
  ~devToolsUpdateActionCreator: ('state) => 'action,
  ~reducer: ('state, 'action) => 'state, 
  ~initial: 'state,
  ~actionSerializer: Types.customSerializer('action, 'serializedAction)=?,
  ~stateSerializer: Types.customSerializer('state, 'serializedState)=?,
  unit
) => ('state, 'action => unit);