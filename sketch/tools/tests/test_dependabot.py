"""Checks on .github/dependabot.yml.

why: every workflow pins its actions by commit SHA (with a `# vX.Y.Z` comment); Dependabot's
github-actions updater is what keeps those pins current. A config that misses the ecosystem or
the workflow folder silently stops all updates.
"""
import os
import unittest

import yaml

REPO = os.path.join(os.path.dirname(__file__), "..", "..", "..")
CONFIG = os.path.join(REPO, ".github", "dependabot.yml")
# why: synced from upstream m5stack (its commits read "chore: sync ..."); not ours to pin or bump
UPSTREAM_WORKFLOWS = {"wecom-digest-forward.yml": "actions/github-script"}


class DependabotTest(unittest.TestCase):
    def setUp(self):
        """Loads .github/dependabot.yml."""
        with open(CONFIG) as f:
            self.config = yaml.safe_load(f)

    def test_config_is_version_2(self):
        """The file uses Dependabot's config schema version 2."""
        self.assertEqual(2, self.config["version"])

    def test_github_actions_are_updated_from_the_repo_root(self):
        """Exactly one github-actions entry, rooted at "/", on a valid schedule."""
        # why "/": Dependabot looks for workflows in <directory>/.github/workflows
        actions = [u for u in self.config["updates"] if u["package-ecosystem"] == "github-actions"]
        self.assertEqual(1, len(actions))
        self.assertEqual("/", actions[0]["directory"])
        self.assertIn(actions[0]["schedule"]["interval"], ("daily", "weekly", "monthly"))

    def test_upstream_workflow_actions_are_ignored(self):
        """Every action used only by an upstream-synced workflow is on the ignore list."""
        # why: a bump of the synced file's action would diverge from upstream's next sync
        actions = next(u for u in self.config["updates"] if u["package-ecosystem"] == "github-actions")
        ignored = {i["dependency-name"] for i in actions.get("ignore", [])}
        for name, action in UPSTREAM_WORKFLOWS.items():
            with self.subTest(workflow=name):
                self.assertIn(action, ignored)


class WorkflowPinTest(unittest.TestCase):
    def test_every_workflow_action_is_sha_pinned(self):
        """Every `uses:` in our own workflows names a full 40-character commit SHA."""
        # why: Dependabot bumps a SHA pin and its version comment together; a tag pin (`@v4`)
        # would float, which deps.txt's "never float" rule forbids
        wf_dir = os.path.join(REPO, ".github", "workflows")
        for name in sorted(set(os.listdir(wf_dir)) - set(UPSTREAM_WORKFLOWS)):
            with open(os.path.join(wf_dir, name)) as f:
                for n, line in enumerate(f, 1):
                    if "uses:" not in line or "uses: ./" in line:
                        continue
                    ref = line.split("@", 1)[1].split()[0] if "@" in line else ""
                    with self.subTest(workflow=name, line=n):
                        self.assertRegex(ref, r"^[0-9a-f]{40}$")


if __name__ == "__main__":
    unittest.main()
