type reduxJsListener = unit => unit;

[@bs.deriving abstract]
type t('state, 'action) = {
  dispatch: 'action => unit,
  subscribe: reduxJsListener => unit,
  getState: unit => 'state,
  replaceReducer: Types.reducer('action, 'state) => t('state, 'action)
};

type storeCreator('action, 'state) = (
  Types.reducer('action, 'state),
  'state,
  Js.Undefined.t(Types.middleware('action, 'state))
) => t('state, 'action);

type storeEnhancer('action, 'state, 'enhancedAction, 'enhancedState) = storeCreator('action, 'state) => storeCreator('enhancedAction, 'enhancedState);
