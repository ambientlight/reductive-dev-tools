## reductive-dev-tools

[reductive](https://github.com/reasonml-community/reductive) and [reason-react](https://github.com/reasonml/reason-react) reducer component integration with [redux-devtools-extension](https://github.com/zalmoxisus/redux-devtools-extension)

![image](assets/demo.gif)

## Installation 
via npm:

```bash
npm install --save-dev reductive-dev-tools
```
then add `reductive-dev-tools` to your "bs-dependencies" inside `bsconfig.json`

## Caveats

1. Add `-bs-g` into `"bsc-flags"` of your **bsconfig.json** to have variant and record field names available inside extension.
2. Prefer variants with constructors to plain (`SomeAction(unit)` to `SomeAction`) since plain varaints do no carry debug metedata with them (represented as numbers in js)
3. Extension will be locked (newly dispatched actions will be ignored) when you jump back in action history.
4. Records inside variants do not carry debug metadata in bucklescript yet, if needed you can tag them manually. See [Additional Tagging](https://github.com/ambientlight/reductive-dev-tools#additional-tagging)

## Supported DevTools Features

| feature | reductive | reducer component |
|---------|-----------|-------------------|
| pause   | ✔         | ✔                 |
| lock    |    [redux-devtools-extension/#618](https://github.com/zalmoxisus/redux-devtools-extension/issues/618)       |     [redux-devtools-extension/#618](https://github.com/zalmoxisus/redux-devtools-extension/issues/618)              |
| persist | ✔         | ✔                 |
| export  | ✔         | ✔                 |
| import  | ✔         | ✔                 |
| jump    | ✔         | ✔                 |
| skip    | ✔         | ✔                 |
| sweep |    [redux-devtools-extension/#618](https://github.com/zalmoxisus/redux-devtools-extension/issues/618)       |     [redux-devtools-extension/#618](https://github.com/zalmoxisus/redux-devtools-extension/issues/618)              |
| reorder |    [redux-devtools-extension/#618](https://github.com/zalmoxisus/redux-devtools-extension/issues/618)       |     [redux-devtools-extension/#618](https://github.com/zalmoxisus/redux-devtools-extension/issues/618)              |
| dispatch| ✔         | ✔                 |
| test    | REDUX ONLY| REDUX ONLY        |
| trace   | REDUX ONLY| REDUX ONLY        | 

## Usage with Reductive
Utilize provided store enhancer `ReductiveDevTools.Connectors.reductiveEnhancer`

```reason
let storeEnhancer =
  ReductiveDevTools.(
    Connectors.reductiveEnhancer(
      Extension.enhancerOptions(~name="MyApp", ()),
    )
  );
  
let storeCreator = storeEnhancer @@ Reductive.Store.create;
```

## Usage with ReactReason reducer component

1. Create devtools connection with `ReductiveDevTools.Connectors.register()`.

	```reason
	/* in your component */
	didMount: self =>
	  ReductiveDevTools.Connectors.register(
	    ~connectionId, 
	    ~component=self, 
	    ~options=ReductiveDevTools.Extension.enhancerOptions(
	      ~actionCreators=Js.Dict.fromList([
	        ("click", (.) => `Click()),
	        ("toogle", (.) => `Toggle(()))
	      ]),
	      ()), 
	    ()),
	```
2. Wrap your reducer into `componentReducerEnhancer` with passed connectionId and handle actions dispatched from the monitor (`DevToolStateUpdate('state)`) to support rewind, revert, import extension features:
	
	```reason
	/* inside your component */
    reducer: ReductiveDevTools.Connectors.componentReducerEnhancer(connectionId, ((action, state) => {
      switch (action) {
      | `Click(_) => ReasonReact.Update({...state, count: state.count + 1})
      | `Toggle(_) => ReasonReact.Update({...state, show: !state.show})
      /* handle the actions dispatched from the dev tools monitor */
      | `DevToolStateUpdate(devToolsState) => ReasonReact.Update(devToolsState)
      }
    })),
	},
	```

3. Unsubscribe when needed with `ReductiveDevTools.Connectors.unsubscribe(~connectionId)`

## Options

```reason
ReductiveDevTools.Extension.enhancerOptions(
  /* the instance name to be showed on the monitor page */
  ~name="SomeTest",
  
  /* action creators functions to be available in the Dispatcher. */
  ~actionCreators=Js.Dict.fromList([
    ("increment", (.) => CounterAction(Increment)),
    ("decrement", (.) => CounterAction(Decrement)),
    ("complexAppend", ((first, last) => StringAction(ComplexAppend(first, last))) |> Obj.magic)
  ]),
  
  /* if more than one action is dispatched in the indicated interval, all new actions will be collected and sent at once */
  ~latency=500,
  
  /* maximum allowed actions to be stored in the history tree. */
  ~maxAge=50,
  
  /* actions types to be hidden / shown in the monitors (while passed to the reducers), If `actionsWhitelist` specified, `actionsBlacklist` is ignored. */
  ~actionsBlacklist=[|"StringAction"|],
  
  /* actions types to be hidden / shown in the monitors (while passed to the reducers), If `actionsWhitelist` specified, `actionsBlacklist` is ignored. */
  ~actionsWhitelist=[|"CounterAction"|],
  
  /* if specified as `true`, whenever there's an exception in reducers, the monitors will show the error message, and next actions will not be dispatched. */
  ~shouldCatchErrors=false,
  
  /* If you want to restrict the extension, specify the features you allow. */
  ~features=ReductiveDevTools.Extension.enhancerFeatures(
    ~pause=true,
    ~persist=true,
    ~export=true,
    ~import=Obj.magic("custom"),
    ~jump=true,
    ~dispatch=true,
    ()),
  ())
```

## Additional Tagging
You can also manually customize serialized objects keys and action names displayed inside extension.
Two common usecases:

1. Labeling variants with constructors.

	```reason
	type routerActions = [
	  | `RouterLocationChanged(list(string), string, string)
	];
	
	open ReductiveDevTools.Utilities;
	Reductive.Store.dispatch(store, 
	  `RouterLocationChanged(url.path, url.hash, url.search)
	    |. labelVariant([|"path", "hash", "search"|]));
	```
2. Labeling record keys for records inside variants (since Records inside variants do not carry debug metadata in bucklescript yet).

	```reason
	type url = {
	  path: list(string),
	  hash: string,
	  search: string,
	};
	type routerActions = [
	  | `RouterLocationChanged(url)
	];
	
	open ReductiveDevTools.Utilities;
	Reductive.Store.dispatch(store, 
	  `RouterLocationChanged(url
	    |. tagRecord([|"path", "hash", "search"|]));
	```
	
And(if really needed) you can override bucklescript debug metadata. Please refer to following definitions in `Utilities`: 

```reason
let tagVariant: ('a, string) => 'a;
let tagPolyVar: ('a, string) => 'a;
let tagRecord: ('a, array(string)) => 'a;
```

## Word Of Caution
Current implementation depends on internal bucklescript representation of debug metadata and variants in js. Changes to it in future may silently break the extension.
