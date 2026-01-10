#!/bin/bash
# Sync fork with upstream master
# Usage: ./sync-fork.sh

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${YELLOW}=== ZoneMinder Fork Sync ===${NC}\n"

# Check if upstream remote exists
if ! git remote | grep -q "^upstream$"; then
    echo -e "${YELLOW}Upstream remote not configured. Adding it now...${NC}"
    git remote add upstream https://github.com/ZoneMinder/ZoneMinder.git
    echo -e "${GREEN}✓ Added upstream remote${NC}\n"
fi

# Show remotes
echo "Configured remotes:"
git remote -v | grep -E "(origin|upstream)"
echo ""

# Fetch from upstream
echo -e "${YELLOW}Fetching from upstream...${NC}"
git fetch upstream

# Check current branch
CURRENT_BRANCH=$(git branch --show-current)
if [ "$CURRENT_BRANCH" != "master" ]; then
    echo -e "${YELLOW}Currently on branch: $CURRENT_BRANCH${NC}"
    echo "Switching to master..."
    git checkout master
fi

# Check for uncommitted changes
if ! git diff-index --quiet HEAD --; then
    echo -e "${RED}ERROR: You have uncommitted changes in master branch.${NC}"
    echo "Please commit or stash them first."
    exit 1
fi

# Show what will change
echo ""
echo "Commits in upstream/master not in your master:"
git log --oneline master..upstream/master | head -10
if [ $(git rev-list --count master..upstream/master) -gt 10 ]; then
    echo "... and $(( $(git rev-list --count master..upstream/master) - 10 )) more"
fi

echo ""
echo "Commits in your master not in upstream/master (these will be removed):"
EXTRA_COMMITS=$(git rev-list --count upstream/master..master)
if [ $EXTRA_COMMITS -eq 0 ]; then
    echo -e "${GREEN}None - your master is already in sync!${NC}"
else
    echo -e "${RED}$EXTRA_COMMITS commits${NC}"
    git log --oneline upstream/master..master | head -5
    if [ $(git rev-list --count upstream/master..master) -gt 5 ]; then
        echo "... and $(( $(git rev-list --count upstream/master..master) - 5 )) more"
    fi
fi

echo ""
read -p "Reset your master to match upstream/master? (y/N) " -n 1 -r
echo ""

if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo "Sync cancelled."
    exit 0
fi

# Reset master to upstream/master
echo -e "${YELLOW}Resetting master to upstream/master...${NC}"
git reset --hard upstream/master

# Force push to origin
echo -e "${YELLOW}Force pushing to origin/master...${NC}"
git push origin master --force

echo ""
echo -e "${GREEN}✓ Fork synchronized successfully!${NC}"
echo -e "${GREEN}✓ Your master branch now matches upstream master.${NC}"
echo ""
echo "Next steps:"
echo "  1. Create feature branch: git checkout -b feature-name"
echo "  2. Make your changes and commit"
echo "  3. Push feature branch: git push origin feature-name"
echo "  4. Create PR from feature branch to upstream/master"
