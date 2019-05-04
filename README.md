# C++ git-cal and git-status

This is inspired by [zsh-git-prompt](https://github.com/olivierverdier/zsh-git-prompt) and [git-cal](https://github.com/k4rthik/git-cal). I only needed something fast. Notice: this readme is a slightly modified clone of their readmes.

So it is written in C++ to make processing the fastest on the planet. There still has to be one (slow) call to `git`, but that's all about it. To make it even faster git-status uses a lockfile+caching mechanism using files in /tmp. If cache is younger than `--refresh-sec` seconds, then it is used rather than invoking `git` again.

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

![screenshot with black theme](https://gitlab.com/cosurgi/zsh-git-cal-status-cpp/raw/master/git-cal-scr.png)

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
    # an example prompt, note letter n in the argument
    PROMPT='%B%m%~%b$(git_super_status n) %# '
    # an example prompt for tracking more repositories, eg. .dotfiles and documents separately
    PROMPT='%B%m%~%b$(git_super_status j)$(git_super_status d)$(git_super_status n) %# '
    ```
3.  Go in a git repository and test it!

## Customisation

- You may redefine the function `git_super_status` (after the `source` statement) to adapt it to your needs (to change the order in which the information is displayed).
- Define the variable `ZSH_THEME_GIT_PROMPT_CACHE` in order to enable caching.
- You may also change a number of variables (which name start with `ZSH_THEME_GIT_PROMPT_`) to change the appearance of the prompt.  Take a look in the file `zshrc.sh` to see how the function `git_super_status` is defined, and what variables are available.
- the `j`, `d` and `n` are used to be able simultaneously track three different repositories (eg. .dotfiles and personal documents separately, along with repository in current directory), see `gitstatus_zshrc.sh` for details.

# git-cal

## Description

* git-cal is a simple C++ code to view commits calendar on command line
* Each block in the graph corresponds to a day and is shaded with one
  of the 5 possible colors, each representing relative number of commits on that day.
* The colors represent quartiles, meaning that red color corresponds to 25% of most active days, and so on.
* Possible to see authors, count contributions in each year, show amount of commits per day.

```
$ git-cal --help
  -h [ --help ]                   display this help.
  --git-dir arg                   The --git-dir for git
  --work-tree arg                 The --work-tree for git
  -a [ --author ] arg             analyse commits of only one selected author,
                                  otherwise all authors are included
  -y [ --only-last-year ]         print only single year of data, skip older
                                  data
  -Y [ --use-calendar-years ]     the calendar will be organized using
                                  `calendar years`, not `days back`
  -s [ --start-with-sunday ]      start week with sunday instead of monday
  -n [ --number-days ]            instead of ◼ put the day of the month (as
                                  in real calendar)
  -c [ --number-commits ]         instead of ◼ put the commit count number
  -N [ --print-authors ] arg (=0) print the commit count and streaks per
                                  author for top N authors
  -S [ --print-streaks ]          print the commit count and streaks for all
                                  authors merged together (or single author if
                                  -a is specified)
  -e [ --include-emails ]         also print the author's email
```

## Screenshots

![screenshot with black theme](https://gitlab.com/cosurgi/zsh-git-cal-status-cpp/raw/master/git-status-scr1.png)
![screenshot with black theme](https://gitlab.com/cosurgi/zsh-git-cal-status-cpp/raw/master/git-status-scr2.png)

BTW, [vim-fugitive](https://github.com/tpope/vim-fugitive) is awesome, and here you can see all its history with top 3 authors:

![screenshot with black theme](https://gitlab.com/cosurgi/zsh-git-cal-status-cpp/raw/master/fugitive.png)

