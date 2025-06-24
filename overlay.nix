self: super: {
  metall = super.stdenv.mkDerivation rec {
    pname = "metall";
    version = "0.31";

    src = super.fetchFromGitHub {
      owner = "LLNL";
      repo = "metall";
      rev = "v0.31";
      sha256 = "sha256-JGOjxFkw4WDXCIEkQinjFx79viBux9LepXCmD5PmKUs=";
    };

    nativeBuildInputs = [ super.cmake ];
    buildInputs = [ super.boost ];

    cmakeFlags = [ "-DCMAKE_BUILD_TYPE=Release" ];
  };
}
