type reduxJsListener = unit => unit;

[@bs.deriving abstract]
type t('state, 'action) = {
  dispatch: 'action => unit,
  subscribe: reduxJsListener => unit,
  getState: unit => 'state,
  replaceReducer: Types.reducer('action, 'state) => t('state, 'action)
};

type middleware('action, 'state) =
  (t('action, 'state), 'action => unit, 'action) => unit;

type _storeCreator('action, 'state) = (
  Types.reducer('action, 'state),
  'state,
  unit
) => t('state, 'action);

type storeEnhancer('action, 'state) = _storeCreator('action, 'state) => _storeCreator('action, 'state);

type storeCreator('action, 'state) = (
  Types.reducer('action, 'state),
  'state,
  Js.Nullable.t(storeEnhancer('action, 'state))
) => t('state, 'action)