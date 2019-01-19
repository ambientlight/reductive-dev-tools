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

let register: (
  ~connectionId: string,
  ~component: ReasonReact.self('state, 'b, [> `DevToolStateUpdate('state) ]),
  ~options: Extension.enhancerOptions('actionCreator)=?, 
  unit
) => unit;

let unsubscribe: (~connectionId: string) => unit;

type componentReducer('state, 'retainedProps, 'action) = ('action, 'state) => ReasonReact.update('state, 'retainedProps, 'action);
let componentReducerEnhancer: (string,
  componentReducer('state, 'retainedProps, ([> `DevToolStateUpdate('state) ] as 'a))) =>
    componentReducer('state, 'retainedProps, 'a);