// TODO: make the next type definitions available at reductive side

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

type storeEnhancer('action, 'state, 'enhancedAction, 'enhancedState) =
  storeCreator('action, 'state) => storeCreator('enhancedAction, 'enhancedState);