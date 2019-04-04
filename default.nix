# default.nix
with import <nixpkgs> {};
stdenv.mkDerivation {
    name = "daq_drv"; # Probably put a more meaningful name here
    buildInputs = [fftwFloat
    libpcap
    ];
    hardeningDisable = [ "all" ];
    #buildInputs = [gcc-unwrapped gcc-unwrapped.out gcc-unwrapped.lib];
    LIBCLANG_PATH = llvmPackages.libclang+"/lib";
}
