pushd "%~dp0"
copy "*.admx" "%SystemRoot%\PolicyDefinitions"
copy "en-US\*.adml" "%SystemRoot%\PolicyDefinitions\en-US"
popd
