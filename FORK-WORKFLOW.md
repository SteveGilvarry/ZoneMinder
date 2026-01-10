# Fork Synchronization Workflow

## Problem
When creating PRs from your fork, you see your changes plus 43+ commits that are already in upstream master.

## Root Cause
Your fork's master branch has diverged from upstream master, creating duplicate commits in the git history.

## Solution: Proper Fork Setup

### One-Time Setup

```bash
# 1. Add upstream remote (main ZoneMinder repo)
git remote add upstream https://github.com/ZoneMinder/ZoneMinder.git

# 2. Verify remotes
git remote -v
# Should show:
# origin    https://github.com/YOUR_USERNAME/ZoneMinder.git (fetch)
# origin    https://github.com/YOUR_USERNAME/ZoneMinder.git (push)
# upstream  https://github.com/ZoneMinder/ZoneMinder.git (fetch)
# upstream  https://github.com/ZoneMinder/ZoneMinder.git (push)
```

### Regular Sync Workflow (Before Starting New Work)

```bash
# 1. Switch to master branch
git checkout master

# 2. Fetch latest from upstream
git fetch upstream

# 3. Reset your master to match upstream master EXACTLY
git reset --hard upstream/master

# 4. Force push to your fork (this is safe for master branch)
git push origin master --force

# Now your fork's master matches upstream master perfectly
```

### Creating Feature Branches

```bash
# 1. Make sure master is synced (see above)
git checkout master
git fetch upstream
git reset --hard upstream/master

# 2. Create feature branch from clean master
git checkout -b feature-branch-name

# 3. Make your changes and commit
git add .
git commit -m "feat: your changes"

# 4. Push feature branch to your fork
git push origin feature-branch-name

# 5. Create PR from feature-branch-name to upstream/master
# On GitHub: compare YOUR_FORK:feature-branch-name -> ZoneMinder:master
```

### Why `git reset --hard` Instead of `git merge`?

**DO NOT use `git merge upstream/master`** - This creates merge commits and diverges your history.

**DO use `git reset --hard upstream/master`** - This makes your master identical to upstream, avoiding duplicates.

### Important Notes

1. **Never commit directly to master** - Always use feature branches
2. **master is expendable** - Your master should always be a clean copy of upstream
3. **Force push to master is safe** - Since you never work directly on master
4. **Keep work in feature branches** - These should never be force-pushed after PR creation

### Quick Reference Script

Save this as `sync-fork.sh`:

```bash
#!/bin/bash
# Sync fork with upstream master

set -e  # Exit on error

echo "Fetching from upstream..."
git fetch upstream

echo "Switching to master..."
git checkout master

echo "Resetting master to upstream/master..."
git reset --hard upstream/master

echo "Force pushing to origin/master..."
git push origin master --force

echo "✓ Fork synchronized successfully!"
echo "Your master branch now matches upstream master."
```

Make it executable:
```bash
chmod +x sync-fork.sh
```

Run before starting new work:
```bash
./sync-fork.sh
```

### Fixing Existing PRs

If you already have a PR with duplicate commits:

```bash
# 1. Sync your master (see above)
./sync-fork.sh

# 2. Checkout your feature branch
git checkout your-feature-branch

# 3. Rebase onto clean master
git rebase master

# 4. Force push the cleaned branch
git push origin your-feature-branch --force

# Your PR will now show only your actual changes
```

### Visual Workflow

```
Upstream (ZoneMinder/ZoneMinder)
    ↓
  master ← fetch & reset --hard
    ↓
Your Fork (origin)
    ↓
  master ← force push
    ↓
feature-branch ← create from master
    ↓
  PR to upstream/master
```

### Common Mistakes to Avoid

❌ `git merge upstream/master` - Creates merge commits and diverges history
❌ Committing directly to master - Makes syncing difficult
❌ Creating PRs from master branch - Includes all commits since fork
❌ Not force-pushing master after reset - Leaves fork diverged

✅ `git reset --hard upstream/master` - Clean sync
✅ Work only in feature branches
✅ Create PRs from feature branches
✅ Force push master after reset (safe since you don't work there)

### Checking Your Sync Status

```bash
# See difference between your master and upstream
git fetch upstream
git log --oneline master..upstream/master  # Shows commits upstream has
git log --oneline upstream/master..master  # Shows commits you have (should be empty)

# If the second command shows commits, you need to sync
```

---

*Follow this workflow and you'll never have duplicate commits in PRs again!*
