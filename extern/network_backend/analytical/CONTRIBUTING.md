# Contributing to the Project

Thank you for considering contribution to the `astra-sim/astra-network-analytical` project. Your efforts are highly appreciated. This document outlines the guidelines for contributing to the project.

## Branches and Forks
This repository (`astra-sim/astra-network-analytical`) maintains two major branches:
- `main`: This branch contains the most stable (deployed) code. This branch gets periodically updated by the project administrator through merging in the release candidate from the `develop` branch. Consequently, no PR should directly be landed at this branch.
- `develop`: Peer-reviewed contributions are merged into this branch. It serves as a release candidate. Therefore, the `develop` should be the base branch of any contributing PR.
- We discourage the creation of additional branches within the repository. All contributions should originate from forks of this repository.

## Pull Request Guidelines
- Please open a pull request (PR) from your forked repository targeting the `develop` branch (not `main`).
- PRs directed at the `main` branch will be rejected and you'll be advised to re-open them targeting the `develop` branch.
- Ensure that all source code follows the formatting guidelines defined in the provided `.clang-format` file. You may consider running `./utils/run_clang_format.sh`.
- All unit tests must pass for a PR to be eligible for merging.
