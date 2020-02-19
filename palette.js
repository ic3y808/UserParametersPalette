var app = angular.module("userprefs", []);
var adsk = undefined;
var lastData = "";

app.controller("mainController", function ($scope) {
  $scope.newTempParameter = { unit: "mm", value: 0, name: "", expression: "0 mm" };
  $scope.possibleUnits = ["cm", "mm"];
  $scope.parameters = { result: [] };
  $scope.isItemValid = function (item) {
    if (item.valid === true) return "btn-success";
    return "btn-warning";
  };

  $scope.getUnitStep = function (unit) {
    switch (unit) {
      case "cm":
        return 1;
      case "mm":
        return 0.1;
    }
    return 1;
  };

  $scope.refreshParameters = function () {
    if (adsk) {
      $("#refreshIcon").addClass("icn-spinner");
      adsk.fusionSendData('update-parameters', "");
    }
  };

  $scope.itemChanged = function () {
    console.log($scope.parameters.result);
    if (adsk) {
      adsk.fusionSendData('change-parameters', JSON.stringify($scope.parameters));
    }
  };

  $scope.addParameter = function () {
    $("#createParameterModal").on("hidden.bs.modal", function () {
      $(this).data("bs.modal", null);
      $scope.newTempParameter = { unit: "mm", value: 0, name: "", expression: "0 mm" };
    });

    $("#createParameterModal").modal();
  };

  $scope.canSaveNewParameter = function () {

    var result = true;
    result = $scope.newTempParameter.name !== '';
    result = $scope.newTempParameter.name.length > 1;
    if ($scope.parameters && $scope.parameters.result && $scope.parameters.result.length > 0) {
      $scope.parameters.result.forEach(function (item) {
        if (item.name.toLowerCase() === $scope.newTempParameter.name.toLowerCase()) {
          result = false;
        }
      });
    }


    return !result;
  };

  $scope.saveNewParameter = function () {
    console.log($scope.newTempParameter);
    $scope.newTempParameter.expression = $scope.newTempParameter.value + " " + $scope.newTempParameter.unit;
    if (!$scope.parameters || !$scope.parameters.result) {
      $scope.parameters = { result: [] };
    }
    $scope.parameters.result.push($scope.newTempParameter);
    $scope.newTempParameter = { unit: "", value: "", name: "", expression: "" };
    console.log($scope.parameters);
    if (adsk) {
      adsk.fusionSendData('change-parameters', JSON.stringify($scope.parameters));
    }
    $("#createParameterModal").modal('hide');
  };


  $("#newParamName").keydown((e) => {
    if (e.code === "Space") e.preventDefault();
  });
  function checkEnter(e) {
    if (e.code === "Enter" || e.code === "NumpadEnter") {
      $scope.saveNewParameter();
    }
  }
  $("#newParamName").keyup((e) => {
    checkEnter(e);
  });
  $("#newParamValue").keyup((e) => {
    checkEnter(e);
  });
});

$(document).ready(function () {
  setTimeout(function () {
    if (adsk) {
      adsk.fusionSendData('update-parameters', "");
    }
    console.log("requesting update");
  }, 500);
});

window.fusionJavaScriptHandler = {
  handle: function (action, data) {
    try {
      var $scope = angular.element(document.getElementById('main-content')).scope();
      var $rootScope = $scope.$root;
      if (action === 'parameter-data') {
        $rootScope.$apply(function () {
          $scope.parameters = JSON.parse(data);
          $("#refreshIcon").removeClass("icn-spinner");
          console.log($scope.parameters);
        });
      }
      else if (action === 'debugger') {
        debugger;
      }
      else {
        return 'Unexpected command type: ' + action;
      }
    } catch (e) {
      console.log(e);
      console.log('exception caught with command: ' + action + ', data: ' + data);
    }
    return 'OK';
  }
};