# C++ git-cal and git-status

This is inspired by https://github.com/olivierverdier/zsh-git-prompt and https://github.com/k4rthik/git-cal. Notice: this readme is a slightly modified clone of their readmes.

It is written in C++ to make processing faster. There is still one (slow) call to `git`, but that's all about slowness. To make it even faster git-status uses a lockfile+caching mechanism using files in /tmp. If cache is younger than `--refresh-sec` seconds, then it is used rather than invoking `git` again.

# git-status

## git-status Examples

The prompt may look like the following:

-   `<master↑3|✚1>`: on branch `master`, ahead of remote by 3 commits, 1 file changed but not staged
-   `<status|●2>`: on branch `status`, 2 files staged
-   `<master|✚7…>`: on branch `master`, 7 files changed, some files untracked
-   `<master|≠2✚3>`: on branch `master`, 2 conflicts, 3 files changed
-   `<experimental↓2↑3|✓>`: on branch `experimental`; your branch has diverged by 3 commits, remote by 2 commits; the repository is otherwise clean
-   `<:70c2952|✓>`: not on any branch; parent commit has hash `70c2952`; the repository is otherwise clean

Here is how it could look like when you are ahead by 1 commit, have 2 staged files, 1 changed but unstaged file, and some untracked files, on branch `master`:

![screenshot with black theme](https://gitlab.com/cosurgi/zsh-git-cal-status-cpp/raw/master/git-status-scr.png")

## Prompt Structure

By default, the general appearance of the prompt is:

```
(<branch><branch tracking>|<local status>)
```

The symbols are as follows:

### Local Status Symbols

|Symbol|Meaning
|------|------|
|✓ |   repository clean
|●n |   there are `n` staged files
|≠n |   there are `n` unmerged files
|✚n |   there are `n` changed but *unstaged* files
|… |   there are some untracked files


### Branch Tracking Symbols

Symbol | Meaning
-------|-------
↑n |   ahead of remote by `n` commits
↓n |   behind remote by `n` commits
↓m↑n |   branches diverged, other by `m` commits, yours by `n` commits

## Install

1.  Clone this repository somewhere on your hard drive.
2.  Source the file `gitstatus_zshrc.sh` from your `~/.zshrc` config file, and
    configure your prompt. So, somewhere in `~/.zshrc`, you should have:

    ```sh
    source path/to/gitstatus_zshrc.sh
    # an example prompt
    PROMPT='%B%m%~%b$(git_super_status n) %# '
    # an example prompt for tracking more repositories
    PROMPT='%B%m%~%b$(git_super_status j)$(git_super_status d)$(git_super_status n) %# '
    ```
3.  Go in a git repository and test it!

## Customisation

- You may redefine the function `git_super_status` (after the `source` statement) to adapt it to your needs (to change the order in which the information is displayed).
- Define the variable `ZSH_THEME_GIT_PROMPT_CACHE` in order to enable caching.
- You may also change a number of variables (which name start with `ZSH_THEME_GIT_PROMPT_`) to change the appearance of the prompt.  Take a look in the file `zshrc.sh` to see how the function `git_super_status` is defined, and what variables are available.
- the `j`, `d` and `n` are used to be able simultaneously track three different repositories, see `gitstatus_zshrc.sh` for details.

# git-cal

### Description


