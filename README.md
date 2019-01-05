# C++ git-cal and git-status

This is inspired by https://github.com/olivierverdier/zsh-git-prompt and https://github.com/k4rthik/git-cal
But it is written in C++ to make processing faster. There is still one (slow) call to `git`. But that's all.

To make it even faster git-status uses a lockfile+caching mechanism using file in /tmp. If cache is younger than `--refresh-sec` seconds, then it is used rather than invoking `git` again.

## git-status Examples

The prompt may look like the following:

-   `<master↑3|✚1>`: on branch `master`, ahead of remote by 3 commits, 1 file changed but not staged
-   `<status|●2>`: on branch `status`, 2 files staged
-   `<master|✚7…>`: on branch `master`, 7 files changed, some files untracked
-   `<master|≠2✚3>`: on branch `master`, 2 conflicts, 3 files changed
-   `<experimental↓2↑3|✓>`: on branch `experimental`; your branch has diverged by 3 commits, remote by 2 commits; the repository is otherwise clean
-   `<:70c2952|✓>`: not on any branch; parent commit has hash `70c2952`; the repository is otherwise clean


