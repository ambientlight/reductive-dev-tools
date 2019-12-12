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

type reduxJsListener = unit => unit;
type reduxJsStore('state, 'action) = {
  dispatch: 'action => unit,
  subscribe: reduxJsListener => unit,
  getState: unit => 'state,
  replaceReducer: reducer('action, 'state) => reduxJsStore('state, 'action)
};

type reduxJsStoreCreator('action, 'state) = (
  reducer('action, 'state),
  'state,
  Js.Undefined.t(middleware('action, 'state))
) => reduxJsStore('state, 'action);

type reduxJsStoreEnhancer('action, 'state) = reduxJsStoreCreator('action, 'state) => reduxJsStoreCreator('action, 'state);

