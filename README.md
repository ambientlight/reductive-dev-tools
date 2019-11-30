## reductive-dev-tools

[![VERSION](https://img.shields.io/npm/v/reductive-dev-tools)](https://www.npmjs.com/package/reductive-dev-tools)
[![LICENSE](https://img.shields.io/github/license/ambientlight/reductive-dev-tools)](https://github.com/ambientlight/reductive-dev-tools/blob/master/LICENSE)
[![ISSUES](https://img.shields.io/github/issues/ambientlight/reductive-dev-tools)](https://github.com/ambientlight/reductive-dev-tools/issues)

[reductive](https://github.com/reasonml-community/reductive) and [reason-react](https://github.com/reasonml/reason-react) reducer component integration with [redux-devtools-extension](https://github.com/zalmoxisus/redux-devtools-extension)

![image](assets/demo.gif)

## Installation 
via npm:

```bash
npm install --save-dev reductive-dev-tools
```
then add `reductive-dev-tools` to your "bs-dependencies" inside `bsconfig.json`.

**Peer depedencies**  
reason-react, reductive, redux-devtools-extension, redux (redux-devtools-extension's peer dep.) should be also installed.

## Caveats

1. Add `-bs-g` into `"bsc-flags"` of your **bsconfig.json** to have variant and record field names available inside extension.
2. Prefer variants with constructors to plain (`SomeAction(unit)` to `SomeAction`) since plain varaints do no carry debug metedata with them (represented as numbers in js)
3. Extension will be locked (newly dispatched actions will be ignored) when you jump back in action history.
4. Records inside variants do not carry debug metadata in bucklescript yet, if needed you can tag them manually. See [Additional Tagging](https://github.com/ambientlight/reductive-dev-tools#additional-tagging)
5. Action names won't be displayed when using extensible variants. [(Extensible variant name becomes "update")](https://github.com/ambientlight/reductive-dev-tools/issues/2)

## Supported DevTools Features

| feature | reductive | react hooks useReducer |
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

## Usage with React Hooks useReducer (jsx3)

Replace `React.useReducer` with `ReductiveDevTools.Connectors.useReducer` and pass options with **unique** component id as name.

```reason
let (state, send) = ReductiveDevTools.Connectors.useReducer(
  ReductiveDevTools.Extension.enhancerOptions(~name="MyComponent", ()),
  reducer,
  yourInitialState);
```

## Usage with ReactReason legacy reducer component (jsx2)
No longer supported. Please install latest from 0.x:

```
npm install --save-dev reductive-dev-tools@0.2.6
```

And refer to [old documentation](https://github.com/ambientlight/reductive-dev-tools/blob/dac77af64763d1aaed584a405c8caeb8b8597272/README.md#usage-with-reactreason-reducer-component).

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
	
This can also be used to override bucklescript debug metadata(if really needed). Definitions are at: [utilities.rei](https://github.com/ambientlight/reductive-dev-tools/blob/a530ea6d09d7facad2b70c061703eff52cfa80b4/src/utilities.rei#L63-L67)

## Word Of Caution
Current implementation depends on internal bucklescript representation of debug metadata and variants in js. Changes to it in future may silently break the extension.
