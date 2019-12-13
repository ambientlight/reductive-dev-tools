/* set of Reductive type definitions which are here until available as part of reductive */
type store('action, 'state) = Reductive.Store.t('action, 'state);
type reducer('action, 'state) = ('state, 'action) => 'state;

type middleware('action, 'state) =
  (store('action, 'state), 'action => unit, 'action) => unit;

type storeCreator('action, 'origin, 'state) =
  (
    ~reducer: reducer('action, 'origin),
    ~preloadedState: 'state,
    ~enhancer: middleware('action, 'state)=?,
    unit
  ) =>
  store('action, 'state);

type storeEnhancer('action, 'origin, 'state) =
  storeCreator('action, 'origin, 'state) => storeCreator('action, 'origin, 'state);

type applyMiddleware('action, 'origin, 'state) =
  middleware('action, 'state) => storeEnhancer('action, 'origin, 'state);

let reductiveEnhancer: Extension.enhancerOptions('actionCreator) => storeEnhancer('action, 'origin, 'state);

let nextEnhancer: (
  ~options: Extension.enhancerOptions('actionCreator), 
  ~devToolsUpdateActionCreator: ('state) => 'action, 
  ~actionSerializer: Types.customSerializer('action, 'serializedAction)=?,
  ~stateSerializer: Types.customSerializer('state, 'serializedState)=?, 
  unit) => Types.storeEnhancer('action, 'state);

let useReducer: (
  Extension.enhancerOptions('actionCreator),  
  ('state, ([> `DevToolStateUpdate('state) ] as 'a)) => 'state, 
  'state
) => ('state, 'a => unit);