<p  align="center">

<img  src="https://github.com/Open-Core-Initiative/sturdynet-openwrt-packages/assets/41849970/bd1e2712-f424-48ba-82de-a1d1eacc0766">

</p>

---

---

## Repository Contents

| File                       | Description                                         |
| :------------------------ | :-------------------------------------------------------- |
| `/PACKAGE_NAME/deploy`               | Name this directory same as your package name. It should contain  all the deploy config and Makefile.                | 
| `47bc41ad54a6601d`       | Public key of the repository. Same as `key-build.pub`               |
| `generate-webpage.sh`      | This file generates an HTML page that is made public to download built packages.        |
| `key-build`       | Private key of the repository. |
| `key-build.pub` | Public key of the repository.        |
| `.github/workflows/main.yml` | Yaml file used by GitHub action to build packages.        |

---

## Prerequisites

1. Github Runner assigned to the project. A runner needs to be running Ubuntu and has Docker installed. You can learn about self-hosted runners [here](#how-to-setup-a-self-hosted-github-runner).
2. SDK docker image of the intended build target created from [sturdynet-openwrt-sdk repository](https://github.com/Open-Core-Initiative/sturdynet-openwrt-sdk) repository.

---

## How to build packages?

1. Create a folder with the same name as your package.
2. Inside this folder create a deploy folder and paste all the configuration files and Makefile.
3. As soon as you commit, it will trigger the build process.

---

## How to access the packages that are built?

To access the built packages, just go to the link: https://open-core-initiative.github.io/sturdynet-openwrt-packages/BRANCH_NAME_HERE

For example, if want to access **ipq40xx** packages, go to: https://open-core-initiative.github.io/sturdynet-openwrt-packages/ipq40xx

---

## How to setup a self-hosted Github runner?

To create a self-hosted runner for this project we need to have a Ubuntu running instance with [Docker installed](https://docs.docker.com/engine/install/ubuntu/#install-using-the-repository). AWS EC2 instance recommended.

1. Go to the Github repository setting -> `Actions` -> `Runners` -> `New self-hosted runner`.
2. Follow the steps for `Linux`
3. In your Ubuntu instance terminal, run `sudo ./svc.sh install`
4. Give access to docker, run `sudo usermod -a -G docker <GITHUB_RUNNER_USER>`
5. Run `sudo ./svc.sh start`