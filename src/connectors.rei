/* set of Reductive type definitions which are here until available as part of reductive */
type store('action, 'state) = Reductive.Store.t('action, 'state);
type reducer('action, 'state) = ('state, 'action) => 'state;

type middleware('action, 'state) =
  (store('action, 'state), 'action => unit, 'action) => unit;

type storeCreator('action, 'state) =
  (
    ~reducer: reducer('action, 'state),
    ~preloadedState: 'state,
    ~enhancer: middleware('action, 'state)=?,
    unit
  ) =>
  store('action, 'state);

type storeEnhancer('action, 'state) =
  storeCreator('action, 'state) => storeCreator('action, 'state);

type applyMiddleware('action, 'state) =
  middleware('action, 'state) => storeEnhancer('action, 'state);

let reductiveEnhancer: Extension.enhancerOptions('actionCreator) => storeEnhancer('action, 'state)

let register: (
  ~connectionId: string,
  ~component: ReasonReact.self('state, 'b, [> `DevToolStateUpdate('state) ]),
  ~options: Extension.enhancerOptions('actionCreator)=?, 
  unit
) => unit;

let send: (~connectionId: string, ~action: ([> `DevToolStateUpdate('state) ]), ~state: 'state) => unit;