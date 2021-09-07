# Formatting style

The OpenSC formatting rules are described in `.clang-format` in the root
directory. It is based on [LLVM](https://llvm.org/docs/CodingStandards.html)
style with couple of modifications:

 * Tabs
   * Tabs are used instead of spaces
   * Tab 8 spaces wide
   * They are used for both indentation and continuation
 * The maximum line width is 110 characters
 * Opening braces follow the condition/expression except for the functions

Examples:

```
void
function_name(int arg)
{
	int var = 0;

	if (arg) {
		var = do_something();
	}
	return var;
}
```

To check your changes if they follow the formatting style (before submitting
a PR), you can use `clang-format` tool or `git-clang-format`, which can check
only the parts of the code you changed in your branch

```
$ git-clang-format --diff --commit upstream/master
```

# Testing locally

To learn how to run the tests from Github actions locally in containers, see
[`containers`](containers/README.md).

# Release process

The release process is described in [OpenSC wiki](https://github.com/OpenSC/OpenSC/wiki/OpenSC-Release-Howto)

TODO tarball signing: https://github.com/OpenSC/OpenSC/issues/1129
