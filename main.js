var app = angular.module("userprefs", []);
var lastData = "";
app.controller("mainController", function ($scope) {
    $scope.possibleUnits = ["cm", "mm"];

    $scope.getUnitStep = function (unit) {
        console.log(unit);
        switch (unit) {
            case "cm":
                return 1;
            case "mm":
                return 0.1;
        }
        return 1;
    };

    $scope.itemChanged = function () {
        console.log($scope.parameters.result[0]);
        adsk.fusionSendData('send', JSON.stringify($scope.parameters));
    };
});

$("#main-content").mouseenter(function () {
    //adsk.fusionSendData('update-parameters', "");
    console.log("requesting update");
});

$(document).ready(function () {
    setTimeout(function () {
        adsk.fusionSendData('update-parameters', "");
        console.log("requesting update");
    }, 500);
});


window.fusionJavaScriptHandler = {
    handle: function (action, data) {
        try {
            var $scope = angular.element(document.getElementById('main-content')).scope();
            var $rootScope = $scope.$root;
            if (action === 'send') {
                $scope.data = data;
            }
            if (action === 'parameter-data') {
                $rootScope.$apply(function () {
                    $scope.parameters = JSON.parse(data);
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